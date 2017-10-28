/*
 * An example showing how to read files using custom I/O routines. This
 * specific example uses PhysFS to read files from zip, 7z, and some other
 * archive formats.
 */

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <limits>
#include <thread>
#include <chrono>

#include "physfs.h"

#include "alure2.h"

namespace
{

// Inherit from std::streambuf to handle custom I/O (PhysFS for this example)
class StreamBuf final : public std::streambuf {
    using BufferArrayT = alure::Array<char_type,4096>;
    BufferArrayT mBuffer;
    PHYSFS_File *mFile;

    int_type underflow() override
    {
        if(mFile && gptr() == egptr())
        {
            // Read in the next chunk of data, and set the read pointers on
            // success
            PHYSFS_sint64 got = PHYSFS_read(mFile,
                mBuffer.data(), sizeof(char_type), mBuffer.size()
            );
            if(got != -1) setg(mBuffer.data(), mBuffer.data(), mBuffer.data()+got);
        }
        if(gptr() == egptr())
            return traits_type::eof();
        return traits_type::to_int_type(*gptr());
    }

    pos_type seekoff(off_type offset, std::ios_base::seekdir whence, std::ios_base::openmode mode) override
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

    pos_type seekpos(pos_type pos, std::ios_base::openmode mode) override
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
    bool open(const char *filename)
    {
        mFile = PHYSFS_openRead(filename);
        if(!mFile) return false;
        return true;
    }

    StreamBuf() : mFile(nullptr)
    { }
    ~StreamBuf() override
    {
        PHYSFS_close(mFile);
        mFile = nullptr;
    }
};

// Inherit from std::istream to use our custom streambuf
class Stream final : public std::istream {
public:
    Stream(const char *filename) : std::istream(new StreamBuf())
    {
        // Set the failbit if the file failed to open.
        if(!(static_cast<StreamBuf*>(rdbuf())->open(filename)))
            clear(failbit);
    }
    ~Stream() override
    { delete rdbuf(); }
};

// Inherit from alure::FileIOFactory to use our custom istream
class FileFactory final : public alure::FileIOFactory {
public:
    FileFactory(const char *argv0)
    {
        // Need to initialize PhysFS before using it
        if(PHYSFS_init(argv0) == 0)
        {
            std::stringstream sstr;
            sstr<< "Failed to initialize PhysFS: "<<PHYSFS_getLastError();
            throw std::runtime_error(sstr.str());
        }

        std::cout<< "Initialized PhysFS, supported archive formats:" <<std::endl;
        for(const PHYSFS_ArchiveInfo **i = PHYSFS_supportedArchiveTypes();*i != NULL;i++)
            std::cout<< "  "<<(*i)->extension<<": "<<(*i)->description <<std::endl;
        std::cout<<std::endl;
    }
    ~FileFactory() override
    {
        PHYSFS_deinit();
    }

    alure::UniquePtr<std::istream> openFile(const alure::String &name) override
    {
        auto stream = alure::MakeUnique<Stream>(name.c_str());
        if(stream->fail()) stream = nullptr;
        return std::move(stream);
    }

    // A PhysFS-specific function to mount a new path to the virtual directory
    // tree.
    static bool Mount(const char *path, const char *mountPoint=nullptr, int append=0)
    {
        std::cout<< "Adding new file source "<<path;
        if(mountPoint) std::cout<< " to "<<mountPoint;
        std::cout<<"..."<<std::endl;

        if(PHYSFS_mount(path, mountPoint, append) == 0)
        {
            std::cerr<< "Failed to add "<<path<<": "<<PHYSFS_getLastError() <<std::endl;
            return false;
        }
        return true;
    }

    static void ListDirectory(std::string&& dir)
    {
        char **files = PHYSFS_enumerateFiles(dir.c_str());
        for(int i = 0;files[i];i++)
        {
            std::string file = dir + files[i];
            if(PHYSFS_isDirectory(file.c_str()))
                ListDirectory(file+"/");
            else
                std::cout<<"  "<<file<<"\n";
        }
        PHYSFS_freeList(files);
    }
};

} // namespace


int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        std::cerr<< "Usage: "<<argv[0]<<" [-device <device name>]"
                    " -add <directory | archive> file_entries ..." <<std::endl;
        return 1;
    }

    // Set our custom factory for file IO. From now on, all filenames given to
    // Alure will be used with our custom factory.
    alure::FileIOFactory::set(alure::MakeUnique<FileFactory>(argv[0]));

    alure::DeviceManager devMgr = alure::DeviceManager::get();

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
        if(alure::StringView("-add") == argv[i] && argc-i > 1)
        {
            FileFactory::Mount(argv[++i]);
            std::cout<<"Available files:\n";
            FileFactory::ListDirectory("/");
            std::cout.flush();
            continue;
        }

        alure::SharedPtr<alure::Decoder> decoder(ctx.createDecoder(argv[i]));
        alure::Source source = ctx.createSource();
        source.play(decoder, 12000, 4);
        std::cout<< "Playing "<<argv[i]<<" ("<<alure::GetSampleTypeName(decoder->getSampleType())<<", "
                                             <<alure::GetChannelConfigName(decoder->getChannelConfig())<<", "
                                             <<decoder->getFrequency()<<"hz)" <<std::endl;

        float invfreq = 1.0f / decoder->getFrequency();
        while(source.isPlaying())
        {
            std::cout<< "\r "<<std::fixed<<std::setprecision(2)<<
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
