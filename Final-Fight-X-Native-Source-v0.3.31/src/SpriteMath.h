#pragma once
#include <algorithm>

namespace ffx {
// OpenBOR stores animation delay in centiseconds. In the classic engine the
// parsed delay is multiplied by GAME_SPEED / 100 and compared against a clock
// that advances GAME_SPEED ticks per second, so the wall-clock duration is
// delay / 100 seconds.
inline float openBorFrameSeconds(int delay){
    return static_cast<float>(std::max(1, delay)) / 100.0f;
}

// OpenBOR pause_add is expressed in engine ticks. Classic GAME_SPEED is 200
// ticks per second, so 20 ticks = 0.1 second of hit stop.
inline float openBorPauseSeconds(int pauseTicks){
    return static_cast<float>(std::max(0, pauseTicks)) / 200.0f;
}

// OpenBOR's offset is the sprite origin/pivot. When horizontally mirrored the
// same world-space origin must stay fixed, so the top-left changes with width.
inline float spriteLeftFromOrigin(float originX,float offsetX,float width,bool facingLeft){
    return facingLeft ? originX + offsetX - width : originX - offsetX;
}

inline float boxLeftFromOrigin(float originX,float offsetX,float boxX,float boxW,bool facingLeft){
    return facingLeft ? originX + offsetX - boxX - boxW : originX - offsetX + boxX;
}
}
