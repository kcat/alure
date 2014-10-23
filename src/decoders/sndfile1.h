#ifndef DECODERS_SNDFILE_H
#define DECODERS_SNDFILE_H

#include "alure2.h"

namespace alure {

class SndFileDecoderFactory : public DecoderFactory {
    virtual Decoder *createDecoder(const std::string &name) final;
};

} // namespace alure

#endif /* DECODERS_SNDFILE_H */
