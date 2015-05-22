#ifndef DECODERS_VORBISFILE_H
#define DECODERS_VORBISFILE_H

#include "alure2.h"

namespace alure {

class VorbisFileDecoderFactory : public DecoderFactory {
    virtual SharedPtr<Decoder> createDecoder(SharedPtr<std::istream> file) final;
};

} // namespace alure

#endif /* DECODERS_VORBISFILE_H */
