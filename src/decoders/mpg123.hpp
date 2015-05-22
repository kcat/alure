#ifndef DECODERS_MPG123_HPP
#define DECODERS_MPG123_HPP

#include "alure2.h"

namespace alure {

class Mpg123DecoderFactory : public DecoderFactory {
    bool mIsInited;

public:
    Mpg123DecoderFactory();
    virtual ~Mpg123DecoderFactory();

    virtual SharedPtr<Decoder> createDecoder(SharedPtr<std::istream> file) final;
};

} // namespace alure

#endif /* DECODERS_MPG123_HPP */
