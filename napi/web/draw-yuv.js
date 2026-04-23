const getGL = (canvas, contextOptions) => {
  let gl = null;
  const names = ['webgl', 'experimental-webgl', 'moz-webgl', 'webkit-3d'];
  let i = 0;
  while (!gl && i < names.length) {
    try {
      gl = contextOptions ? canvas.getContext(names[i], contextOptions) : canvas.getContext(names[i]);
    } catch {
      gl = null;
    }
    if (!gl || typeof gl.getParameter !== 'function') {
      gl = null;
    }
    i += 1;
  }
  return gl;
};

const compileShader = (gl, vertexSource, fragmentSource) => {
  const vertexShader = gl.createShader(gl.VERTEX_SHADER);
  const fragmentShader = gl.createShader(gl.FRAGMENT_SHADER);
  if (!vertexShader || !fragmentShader) throw new Error('createShader failed');
  gl.shaderSource(vertexShader, vertexSource);
  gl.shaderSource(fragmentShader, fragmentSource);
  gl.compileShader(vertexShader);
  gl.compileShader(fragmentShader);
  const okV = gl.getShaderParameter(vertexShader, gl.COMPILE_STATUS);
  const okF = gl.getShaderParameter(fragmentShader, gl.COMPILE_STATUS);
  if (!okV || !okF) throw new Error('compile shader failed');
  const program = gl.createProgram();
  if (!program) throw new Error('createProgram failed');
  gl.attachShader(program, vertexShader);
  gl.attachShader(program, fragmentShader);
  gl.linkProgram(program);
  gl.useProgram(program);
  if (!gl.getProgramParameter(program, gl.LINK_STATUS)) throw new Error('link program failed');
  gl.program = program;
  return program;
};

export class DrawYuv {
  constructor(canvas) {
    this.canvas = canvas;
    this.gl = getGL(canvas);
    this.attribute = {};
    this.uniform = {};
    this.mat4 = [
      1.16438, 0.0, 1.79274, -0.97295,
      1.16438, -0.21325, -0.53291, 0.30148,
      1.16438, 2.1124, 0.0, -1.1334,
      0, 0, 0, 1,
    ];
    this.init();
  }

  play({ width, height, yData, uData, vData }) {
    this.gl.viewport(0, 0, width, height);
    this.gl.pixelStorei(this.gl.UNPACK_FLIP_Y_WEBGL, 1);
    const buffer = this.gl.createBuffer();
    const point = [0, 1, -1, 1, 0, 0, -1, -1, 1, 1, 1, 1, 1, 0, 1, -1];
    const bytes = new Float32Array(point);
    const fsize = bytes.BYTES_PER_ELEMENT;
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, buffer);
    this.gl.bufferData(this.gl.ARRAY_BUFFER, bytes, this.gl.STATIC_DRAW);
    this.gl.vertexAttribPointer(this.attribute.a_Texture, 2, this.gl.FLOAT, false, 4 * fsize, 0);
    this.gl.enableVertexAttribArray(this.attribute.a_Texture);
    this.gl.vertexAttribPointer(this.attribute.a_Position, 2, this.gl.FLOAT, false, 4 * fsize, 2 * fsize);
    this.gl.enableVertexAttribArray(this.attribute.a_Position);

    const t0 = this.createTexture();
    const t1 = this.createTexture();
    const t2 = this.createTexture();
    this.gl.uniform1i(this.uniform.ySampler, 0);
    this.gl.uniform1i(this.uniform.uSampler, 1);
    this.gl.uniform1i(this.uniform.vSampler, 2);
    this.gl.uniformMatrix4fv(this.uniform.yuv2rgb, false, new Float32Array(this.mat4));

    this.gl.activeTexture(this.gl.TEXTURE0);
    this.gl.bindTexture(this.gl.TEXTURE_2D, t0);
    this.gl.texImage2D(this.gl.TEXTURE_2D, 0, this.gl.LUMINANCE, width, height, 0, this.gl.LUMINANCE, this.gl.UNSIGNED_BYTE, yData);
    this.gl.activeTexture(this.gl.TEXTURE1);
    this.gl.bindTexture(this.gl.TEXTURE_2D, t1);
    this.gl.texImage2D(this.gl.TEXTURE_2D, 0, this.gl.LUMINANCE, width >> 1, height >> 1, 0, this.gl.LUMINANCE, this.gl.UNSIGNED_BYTE, uData);
    this.gl.activeTexture(this.gl.TEXTURE2);
    this.gl.bindTexture(this.gl.TEXTURE_2D, t2);
    this.gl.texImage2D(this.gl.TEXTURE_2D, 0, this.gl.LUMINANCE, width >> 1, height >> 1, 0, this.gl.LUMINANCE, this.gl.UNSIGNED_BYTE, vData);
    this.gl.drawArrays(this.gl.TRIANGLE_STRIP, 0, 4);
  }

  init() {
    compileShader(this.gl, this.vertexShader(), this.fragmentShader());
    this.gl.clearColor(0, 0, 0, 1);
    this.gl.clear(this.gl.COLOR_BUFFER_BIT);
    this.attribute.a_Position = this.gl.getAttribLocation(this.gl.program, 'a_Position');
    this.attribute.a_Texture = this.gl.getAttribLocation(this.gl.program, 'a_Texture');
    this.uniform.ySampler = this.gl.getUniformLocation(this.gl.program, 'ySampler');
    this.uniform.uSampler = this.gl.getUniformLocation(this.gl.program, 'uSampler');
    this.uniform.vSampler = this.gl.getUniformLocation(this.gl.program, 'vSampler');
    this.uniform.yuv2rgb = this.gl.getUniformLocation(this.gl.program, 'YUV2RGB');
  }

  createTexture() {
    const t = this.gl.createTexture();
    this.gl.bindTexture(this.gl.TEXTURE_2D, t);
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MAG_FILTER, this.gl.NEAREST);
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MIN_FILTER, this.gl.NEAREST);
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_WRAP_S, this.gl.CLAMP_TO_EDGE);
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_WRAP_T, this.gl.CLAMP_TO_EDGE);
    return t;
  }

  vertexShader() {
    return `
      attribute vec4 a_Position;
      attribute vec2 a_Texture;
      varying vec2 v_Texture;
      void main() {
        gl_Position = a_Position;
        v_Texture = a_Texture;
      }
    `;
  }

  fragmentShader() {
    return `
      precision mediump float;
      uniform sampler2D ySampler;
      uniform sampler2D uSampler;
      uniform sampler2D vSampler;
      uniform mat4 YUV2RGB;
      varying vec2 v_Texture;
      void main(void) {
        float y = texture2D(ySampler, v_Texture).r;
        float u = texture2D(uSampler, v_Texture).r;
        float v = texture2D(vSampler, v_Texture).r;
        gl_FragColor = vec4(y, u, v, 1.0) * YUV2RGB;
      }
    `;
  }
}
