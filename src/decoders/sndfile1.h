#ifndef DECODERS_SNDFILE_H
#define DECODERS_SNDFILE_H

#include "alure2.h"

namespace alure {

class SndFileDecoderFactory : public DecoderFactory {
    virtual SharedPtr<Decoder> createDecoder(SharedPtr<std::istream> file) final;
};

} // namespace alure

#endif /* DECODERS_SNDFILE_H */
