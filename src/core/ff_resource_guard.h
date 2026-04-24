#ifndef FF_RESOURCE_GUARD_H_
#define FF_RESOURCE_GUARD_H_

#include "ffdecode_types.h"

namespace ffwasm {

void ReleaseCodecContext(FFCodecContext*& ioCodecCtx);

class FFResourceGuard {
public:
	FFResourceGuard(FFVideoState** state, FFBufferData** buffer);
	void Dismiss();
	void ReleaseAll();
	~FFResourceGuard();

private:
	FFVideoState** state_ = nullptr;
	FFBufferData** buffer_ = nullptr;
	bool active_ = true;
};

}  // namespace ffwasm

#endif  // FF_RESOURCE_GUARD_H_
