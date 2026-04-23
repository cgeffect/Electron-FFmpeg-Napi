'use strict';

const path = require('path');
const bindings = require('bindings');

const native = bindings({
  bindings: 'ffmpeg_player_napi',
  module_root: __dirname,
});

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
  openFromBuffer,
  addonPath: path.join(__dirname, 'build/Release/ffmpeg_player_napi.node'),
};
