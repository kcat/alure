/*
 * An example showing how to use an external decoder to play files through the
 * DUMB library.
 */

#include <iostream>
#include <iomanip>
#include <cstring>
#include <limits>
#include <thread>
#include <chrono>

#include "dumb.h"

#include "alure2.h"

namespace
{

// Some I/O function callback wrappers for DUMB to read from an std::istream
static int cb_skip(void *user_data, long offset)
{
    std::istream *stream = static_cast<std::istream*>(user_data);
    stream->clear();

    if(stream->seekg(offset, std::ios_base::cur))
        return 0;
    return 1;
}

static long cb_read(char *ptr, long size, void *user_data)
{
    std::istream *stream = static_cast<std::istream*>(user_data);
    stream->clear();

    stream->read(ptr, size);
    return stream->gcount();
}

static int cb_read_char(void *user_data)
{
    std::istream *stream = static_cast<std::istream*>(user_data);
    stream->clear();

    unsigned char ret;
    stream->read(reinterpret_cast<char*>(&ret), 1);
    if(stream->gcount() > 0) return ret;
    return -1;
}

static int cb_seek(void *user_data, long n)
{
    std::istream *stream = static_cast<std::istream*>(user_data);
    stream->clear();

    if(!stream->seekg(n))
        return 1;
    return 0;
}

static long cb_get_size(void *user_data)
{
    std::istream *stream = static_cast<std::istream*>(user_data);
    stream->clear();

    long len = -1;
    std::streampos pos = stream->tellg();
    if(pos != -1 && stream->seekg(0, std::ios::end))
    {
        len = stream->tellg();
        stream->seekg(pos);
    }
    return len;
}


// Inherit from alure::Decoder to make a custom decoder (DUMB for this example)
class DumbDecoder : public alure::Decoder {
    alure::UniquePtr<std::istream> mFile;

    alure::UniquePtr<DUMBFILE_SYSTEM> mDfs;
    DUMBFILE *mDumbfile;
    DUH *mDuh;
    DUH_SIGRENDERER *mRenderer;

    alure::SampleType mSampleType;
    ALuint mFrequency;

    alure::Vector<sample_t> mSampleBuf;
    uint64_t mStreamPos;

public:
    DumbDecoder(alure::UniquePtr<std::istream> file, alure::UniquePtr<DUMBFILE_SYSTEM> dfs,
                DUMBFILE *dfile, DUH *duh, DUH_SIGRENDERER *renderer, alure::SampleType stype,
                ALuint srate)
      : mFile(std::move(file)), mDfs(std::move(dfs)), mDumbfile(dfile), mDuh(duh)
      , mRenderer(renderer), mSampleType(stype), mFrequency(srate), mStreamPos(0)
    { }
    ~DumbDecoder() override final
    {
        duh_end_sigrenderer(mRenderer);
        mRenderer = nullptr;

        unload_duh(mDuh);
        mDuh = nullptr;

        dumbfile_close(mDumbfile);
        mDumbfile = nullptr;
    }

    ALuint getFrequency() const override final
    { return mFrequency; }
    alure::ChannelConfig getChannelConfig() const override final
    {
        // We always have DUMB render to stereo
        return alure::ChannelConfig::Stereo;
    }
    alure::SampleType getSampleType() const override final
    {
        // DUMB renders to 8.24 normalized fixed point, which we convert to
        // 32-bit float or signed 16-bit samples
        return mSampleType;
    }

    uint64_t getLength() const override final
    {
        // Modules have no explicit length, they just keep playing as long as
        // more samples get generated.
        return 0;
    }

    uint64_t getPosition() const override final
    {
        return mStreamPos;
    }

    bool seek(uint64_t) override final
    {
        // Cannot seek
        return false;
    }

    std::pair<uint64_t,uint64_t> getLoopPoints() const override final
    {
        // No loop points
        return std::make_pair(0, 0);
    }

    ALuint read(ALvoid *ptr, ALuint count) override final
    {
        ALuint ret = 0;

        mSampleBuf.resize(count*2);
        std::array<sample_t*,1> samples{{mSampleBuf.data()}};

        dumb_silence(samples[0], mSampleBuf.size());
        ret = duh_sigrenderer_generate_samples(mRenderer, 1.0f, 65536.0f/mFrequency, count,
                                               samples.data());
        if(mSampleType == alure::SampleType::Float32)
        {
            ALfloat *out = reinterpret_cast<ALfloat*>(ptr);
            for(ALuint i = 0;i < ret*2;i++)
                out[i] = (ALfloat)samples[0][i] * (1.0f/8388608.0f);
        }
        else
        {
            ALshort *out = reinterpret_cast<ALshort*>(ptr);
            for(ALuint i = 0;i < ret*2;i++)
            {
                sample_t smp = samples[0][i]>>8;
                if(smp < -32768) smp = -32768;
                else if(smp > 32767) smp = 32767;
                out[i] = smp;
            }
        }
        mStreamPos += ret;

        return ret;
    }
};

// Inherit from alure::DecoderFactory to use our custom decoder
class DumbFactory : public alure::DecoderFactory {
    alure::SharedPtr<alure::Decoder> createDecoder(alure::UniquePtr<std::istream> &file) override final
    {
        static const std::array<DUH*(*)(DUMBFILE*),3> init_funcs{{
            dumb_read_it, dumb_read_xm, dumb_read_s3m
        }};

        auto dfs = alure::MakeUnique<DUMBFILE_SYSTEM>();
        dfs->open = nullptr;
        dfs->skip = cb_skip;
        dfs->getc = cb_read_char;
        dfs->getnc = cb_read;
        dfs->close = nullptr;
        dfs->seek = cb_seek;
        dfs->get_size = cb_get_size;

        DUMBFILE *dfile = dumbfile_open_ex(file.get(), dfs.get());
        if(!dfile) return nullptr;

        alure::Context ctx = alure::Context::GetCurrent();
        alure::SampleType stype = alure::SampleType::Float32;
        if(!ctx.isSupported(alure::ChannelConfig::Stereo, stype))
            stype = alure::SampleType::Int16;
        ALuint freq = ctx.getDevice().getFrequency();

        for(auto init : init_funcs)
        {
            DUH *duh;
            if((duh=init(dfile)) != nullptr)
            {
                DUH_SIGRENDERER *renderer;
                if((renderer=duh_start_sigrenderer(duh, 0, 2, 0)) != nullptr)
                    return alure::MakeShared<DumbDecoder>(
                        std::move(file), std::move(dfs), dfile, duh, renderer, stype, freq
                    );

                unload_duh(duh);
                duh = nullptr;
            }

            dumbfile_seek(dfile, 0, SEEK_SET);
        }

        DUH *duh;
        if((duh=dumb_read_mod(dfile, 1)) != nullptr)
        {
            DUH_SIGRENDERER *renderer;
            if((renderer=duh_start_sigrenderer(duh, 0, 2, 0)) != nullptr)
                return alure::MakeShared<DumbDecoder>(
                    std::move(file), std::move(dfs), dfile, duh, renderer, stype, freq
                );

            unload_duh(duh);
            duh = nullptr;
        }

        dumbfile_close(dfile);
        return nullptr;
    }
};

} // namespace


int main(int argc, char *argv[])
{
    // Set our custom factory for decoding modules.
    alure::RegisterDecoder("dumb", alure::MakeUnique<DumbFactory>());

    alure::DeviceManager &devMgr = alure::DeviceManager::get();

    int fileidx = 1;
    alure::Device dev;
    if(argc > 3 && strcmp(argv[1], "-device") == 0)
    {
        fileidx = 3;
        dev = devMgr.openPlayback(argv[2], std::nothrow);
        if(!dev)
            std::cerr<< "Failed to open \""<<argv[2]<<"\" - trying default" <<std::endl;
    }
    if(!dev)
        dev = devMgr.openPlayback();
    std::cout<< "Opened \""<<dev.getName()<<"\"" <<std::endl;

    alure::Context ctx = dev.createContext();
    alure::Context::MakeCurrent(ctx);

    for(int i = fileidx;i < argc;i++)
    {
        alure::SharedPtr<alure::Decoder> decoder(ctx.createDecoder(argv[i]));
        alure::Source source = ctx.createSource();
        source.play(decoder, 12000, 4);
        std::cout<< "Playing "<<argv[i]<<" ("<<alure::GetSampleTypeName(decoder->getSampleType())<<", "
                                             <<alure::GetChannelConfigName(decoder->getChannelConfig())<<", "
                                             <<decoder->getFrequency()<<"hz)" <<std::endl;

        float invfreq = 1.0f / decoder->getFrequency();
        while(source.isPlaying())
        {
            std::cout<< "\r "<<std::setiosflags(std::ios::fixed)<<std::setprecision(2)<<
                        source.getSecOffset().count()<<" / "<<(decoder->getLength()*invfreq);
            std::cout.flush();
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            ctx.update();
        }
        std::cout<<std::endl;

        source.release();
        decoder.reset();
    }

    alure::Context::MakeCurrent(nullptr);
    ctx.destroy();
    dev.close();

    return 0;
}
