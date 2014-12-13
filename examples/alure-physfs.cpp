#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <time.h>
#include <stdint.h>
inline void Sleep(uint32_t ms)
{
    struct timespec ts, rem;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    while(nanosleep(&ts, &rem) == -1 && errno == EINTR)
        ts = rem;
}
#endif

#include <iostream>
#include <iomanip>
#include <cstring>
#include <limits>

#include "physfs.h"

#include "alure2.h"

namespace
{

// Inherit from std::streambuf to handle custom I/O (PhysFS for this example)
class StreamBuf : public std::streambuf {
    static const size_t sBufferSize = 4096;

    PHYSFS_File *mFile;
    char mBuffer[sBufferSize];

    virtual int_type underflow()
    {
        if(mFile && gptr() == egptr())
        {
            // Read in the next chunk of data, and set the read pointers on
            // success
            PHYSFS_sint64 got = PHYSFS_read(mFile, mBuffer, sizeof(mBuffer[0]), sBufferSize);
            if(got != -1) setg(mBuffer, mBuffer, mBuffer+got);
        }
        if(gptr() == egptr())
            return traits_type::eof();
        return (*gptr())&0xFF;
    }

    virtual pos_type seekoff(off_type offset, std::ios_base::seekdir whence, std::ios_base::openmode mode)
    {
        if(!mFile || (mode&std::ios_base::out) || !(mode&std::ios_base::in))
            return traits_type::eof();

        // PhysFS only seeks using absolute offsets, so we have to convert cur-
        // and end-relative offsets.
        PHYSFS_sint64 fpos;
        switch(whence)
        {
            case std::ios_base::beg:
                break;

            case std::ios_base::cur:
                // Need to offset for the read pointers with std::ios_base::cur
                // regardless
                offset -= off_type(egptr()-gptr());
                if((fpos=PHYSFS_tell(mFile)) == -1)
                    return traits_type::eof();
                offset += fpos;
                break;

            case std::ios_base::end:
                if((fpos=PHYSFS_fileLength(mFile)) == -1)
                    return traits_type::eof();
                offset += fpos;
                break;

            default:
                return traits_type::eof();
        }
        // Range check - absolute offsets cannot be less than 0, nor be greater
        // than PhysFS's offset type.
        if(offset < 0 || offset >= std::numeric_limits<PHYSFS_sint64>::max())
            return traits_type::eof();
        if(PHYSFS_seek(mFile, PHYSFS_sint64(offset)) == 0)
            return traits_type::eof();
        // Clear read pointers so underflow() gets called on the next read
        // attempt.
        setg(0, 0, 0);
        return offset;
    }

    virtual pos_type seekpos(pos_type pos, std::ios_base::openmode mode)
    {
        // Simplified version of seekoff
        if(!mFile || (mode&std::ios_base::out) || !(mode&std::ios_base::in))
            return traits_type::eof();

        if(pos < 0 || pos >= std::numeric_limits<PHYSFS_sint64>::max())
            return traits_type::eof();
        if(PHYSFS_seek(mFile, PHYSFS_sint64(pos)) == 0)
            return traits_type::eof();
        setg(0, 0, 0);
        return pos;
    }

public:
    bool isOpen() const
    {
        return mFile != nullptr;
    }

    StreamBuf(const char *filename)
      : mFile(nullptr)
    {
        mFile = PHYSFS_openRead(filename);
    }
    virtual ~StreamBuf()
    {
        if(mFile)
            PHYSFS_close(mFile);
        mFile = nullptr;
    }
};

// Inherit from std::istream to use our custom streambuf
class Stream : public std::istream {
public:
    Stream(const char *filename)
      : std::istream(new StreamBuf(filename))
    {
        // Set the failbit if the file failed to open.
        if(!(static_cast<StreamBuf*>(rdbuf())->isOpen()))
            clear(failbit);
    }
    virtual ~Stream()
    { delete rdbuf(); }
};

// Inherit from alure::FileIOFactory to use our custom istream
class FileFactory : public alure::FileIOFactory {
    virtual std::unique_ptr<std::istream> openFile(const std::string &name)
    {
        std::unique_ptr<Stream> stream(new Stream(name.c_str()));
        if(stream->fail()) stream.reset();
        return std::move(stream);
    }
};

} // namespace


int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        std::cerr<< "Usage: "<<argv[0]<<" -add <directory | archive> file_entries ..." <<std::endl;
        return 1;
    }

    // Need to initialize PhysFS before using it
    if(PHYSFS_init(argv[0]) == 0)
    {
        std::cerr<< "Failed to initialize PhysFS: "<<PHYSFS_getLastError() <<std::endl;
        return 1;
    }
    // Set our custom factory for file IO (Alure takes ownership of the factory
    // instance). From now on, all filenames given to Alure will be used with
    // our custom factory.
    alure::FileIOFactory::set(std::unique_ptr<alure::FileIOFactory>(new FileFactory));

    std::cout<< "Initialized PhysFS, supported archive formats:" <<std::endl;
    for(const PHYSFS_ArchiveInfo **i = PHYSFS_supportedArchiveTypes();*i != NULL;i++)
        std::cout<< "  "<<(*i)->extension<<": "<<(*i)->description <<std::endl;

    alure::DeviceManager *devMgr = alure::DeviceManager::get();

    alure::Device *dev = devMgr->openPlayback();
    std::cout<< "Opened \""<<dev->getName(alure::PlaybackDevType_Basic)<<"\"" <<std::endl;

    alure::Context *ctx = dev->createContext();
    alure::Context::MakeCurrent(ctx);

    for(int i = 1;i < argc;i++)
    {
        if(strcasecmp(argv[i], "-add") == 0 && argc-i > 1)
        {
            // Mount a new path for PhysFS to access files from
            if(PHYSFS_mount(argv[++i], NULL, 0) == 0)
                std::cerr<< "Failed to add "<<argv[i]<<": "<<PHYSFS_getLastError() <<std::endl;
            continue;
        }

        std::unique_ptr<alure::Decoder> decoder(ctx->createDecoder(argv[i]));
        alure::Source *source = ctx->getSource();
        source->play(decoder.get(), 32768, 4);
        std::cout<< "Playing "<<argv[i]<<" ("<<alure::GetSampleTypeName(decoder->getSampleType())<<", "
                                             <<alure::GetSampleConfigName(decoder->getSampleConfig())<<", "
                                             <<decoder->getFrequency()<<"hz)" <<std::endl;

        float invfreq = 1.0f / decoder->getFrequency();
        while(source->isPlaying())
        {
            std::cout<< "\r "<<std::setiosflags(std::ios::fixed)<<std::setprecision(2)<<
                        (source->getOffset()*invfreq)<<" / "<<(decoder->getLength()*invfreq);
            std::cout.flush();
            Sleep(25);
            ctx->update();
        }
        std::cout<<std::endl;

        source->release();
        source = nullptr;
        decoder.reset();
    }

    alure::Context::MakeCurrent(nullptr);
    ctx->destroy();
    ctx = 0;
    dev->close();
    dev = 0;

    // Unset the custom factory so we can safely deinitialize PhysFS (Alure
    // returns the previously set factory, which implicitly gets deleted since
    // we don't store the returned unique_ptr).
    alure::FileIOFactory::set(nullptr);
    PHYSFS_deinit();
    return 0;
}
