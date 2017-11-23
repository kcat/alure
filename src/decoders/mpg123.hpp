#ifndef DECODERS_MPG123_HPP
#define DECODERS_MPG123_HPP

#include "alure2.h"

namespace alure {

class Mpg123DecoderFactory final : public DecoderFactory {
    bool mIsInited;

public:
    Mpg123DecoderFactory() noexcept;
    ~Mpg123DecoderFactory() override;

    SharedPtr<Decoder> createDecoder(UniquePtr<std::istream> &file) noexcept override;
};

} // namespace alure

#endif /* DECODERS_MPG123_HPP */
