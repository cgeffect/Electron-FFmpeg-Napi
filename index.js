'use strict';

const path = require('path');
const bindings = require('bindings');

const native = bindings({
  bindings: 'ffmpeg_player_napi',
  module_root: __dirname,
});

class NativePlayer {
  constructor() {
    this.handle = 0n;
    this.info = null;
  }

  openFromBuffer(buffer) {
    if (!Buffer.isBuffer(buffer)) {
      throw new TypeError('openFromBuffer(buffer): buffer must be Buffer');
    }
    this.close();
    this.handle = native.open(buffer);
    this.info = native.getInfo(this.handle);
    return this.info;
  }

  decodeAt(ptsMs) {
    if (!this.handle) {
      return null;
    }
    return native.decodeFrame(this.handle, ptsMs);
  }

  seekTo(ptsMs) {
    if (!this.handle) {
      return null;
    }
    return native.seekFrame(this.handle, ptsMs);
  }

  setHoldSeek(seek) {
    if (!this.handle) {
      return 0;
    }
    return native.holdSeek(this.handle, Boolean(seek));
  }

  close() {
    if (this.handle) {
      native.close(this.handle);
      this.handle = 0n;
      this.info = null;
    }
  }
}

function openFromBuffer(buffer) {
  if (!Buffer.isBuffer(buffer)) {
    throw new TypeError('openFromBuffer(buffer): buffer must be Buffer');
  }
  const handle = native.open(buffer);
  const info = native.getInfo(handle);
  return { handle, info };
}

module.exports = {
  ...native,
  NativePlayer,
  openFromBuffer,
  addonPath: path.join(__dirname, 'build/Release/ffmpeg_player_napi.node'),
};
