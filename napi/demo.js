'use strict';

const fs = require('fs');
const path = require('path');
const addon = require('./index');

function run() {
  const filePath = process.argv[2] || path.join(__dirname, '../res/640.mp4');
  if (filePath === '/absolute/path/to/your.mp4') {
    console.error('Please replace placeholder path with a real video file path.');
    console.error('Example: npm run demo -- "/Users/you/Videos/sample.mp4"');
    process.exit(1);
  }
  if (!fs.existsSync(filePath)) {
    console.error(`Video file not found: ${filePath}`);
    console.error('Example: npm run demo -- "/Users/you/Videos/sample.mp4"');
    process.exit(1);
  }
  const data = fs.readFileSync(filePath);

  const { handle, info } = addon.openFromBuffer(data);
  console.log('video info:', info);

  const frame0 = addon.decodeFrame(handle, 0);
  console.log('frame@0ms:', frame0 ? {
    width: frame0.width,
    height: frame0.height,
    y: frame0.y.length,
    u: frame0.u.length,
    v: frame0.v.length,
    lineY: frame0.lineY,
  } : null);

  const frame1s = addon.seekFrame(handle, 1000);
  console.log('frame@1000ms:', frame1s ? {
    width: frame1s.width,
    height: frame1s.height,
    y: frame1s.y.length,
    u: frame1s.u.length,
    v: frame1s.v.length,
    lineY: frame1s.lineY,
  } : null);

  addon.close(handle);
}

run();
