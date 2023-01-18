export class NativeImage {
  private readonly source: any
  private readonly _width: number | undefined
  private readonly _height: number | undefined

  constructor(source: any, width?: number, height?: number) {
    this.source = source
    this._width = width
    this._height = height
  }

  width(): number {
    if (this._width) {
      return this._width
    }
    return this.source instanceof HTMLVideoElement ? this.source.videoWidth : this.source.naturalWidth
  }

  height(): number {
    if (this._height) {
      return this._height
    }
    return this.source instanceof HTMLVideoElement ? this.source.videoHeight : this.source.naturalHeight
  }

  getImageData() {
    const canvas = document.createElement('canvas')
    const ctx = canvas.getContext('2d') as CanvasRenderingContext2D

    canvas.width = this.width()
    canvas.height = this.height()

    ctx.drawImage(this.source, 0, 0)
    const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height)

    return imageData
  }

  upload(GL: any) {
    const gl = GL.currentContext.GLctx
    gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, true)
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, this.source)
  }
}
