export interface IYUVObject {
  width: number
  height: number
  yData: Uint8Array
  uData: Uint8Array
  vData: Uint8Array
}

export interface IVideoInfo {
  width: number
  height: number
  duration: number
  fps: number
}

export type DecodeYUVCallback = (
  yAddr: number,
  uAddr: number,
  vAddr: number,
  yLen: number,
  uLen: number,
  vLen: number,
  width: number,
  height: number,
  time: number
) => void

export interface IFFModule {
  HEAPU8?: Uint8Array
  wasmMemory?: WebAssembly.Memory
  addFunction: (func: (...args: number[]) => void, sig: string) => number
  removeFunction?: (ptr: number) => void
  _malloc: (size: number) => number
  _free: (ptr: number) => void
  _ffwasm_decode_open: (dataPtr: number, length: number, infoCallback: number) => number
  _ffwasm_decode_frame: (handle: number, pts: number, decodeCallback: number) => number
  _ffwasm_seek_frame: (handle: number, pts: number, decodeCallback: number) => number
  _ffwasm_hold_seek: (handle: number, seek: number) => number
  _ffwasm_decode_free: (handle: number) => number
}
