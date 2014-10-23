#ifndef DECODERS_MPG123_H
#define DECODERS_MPG123_H

#include "alure2.h"

namespace alure {

class Mpg123DecoderFactory : public DecoderFactory {
    virtual Decoder *createDecoder(const std::string &name) final;
};

} // namespace alure

#endif /* DECODERS_MPG123_H */
