
import ModuleKit from '../ffmpegkit/libffmpeg.js'

function Uint8ArrayToString(fileData:any){
    var dataString = "";
    for (var i = 0; i < fileData.length; i++) {
        dataString += String.fromCharCode(fileData[i]);
    }
    return dataString
}

ModuleKit({ locateFile: () => '../ffmpegkit/libffmpeg.wasm' }).then((module: any) => {
    /**
     * 1. 注册打开解码器
     * 2. 注册decodeCallback
     * 3. 输入解码数据
     */
    function decodeCallback(addr_y:any,
                    addr_u:any, 
                    addr_v:any,
                    stride_y:any,
                    stride_u:any,
                    stride_v:any,
                    width:any,
                    height:any,
                    pts:any) {
        console.log("width = " + width + ", height = " + height + ", pts = " + pts)
        //从wasm -> js

        // debugger
        let out_y = module.HEAPU8.subarray(addr_y, addr_y + stride_y * height)
        let out_u = module.HEAPU8.subarray(addr_u, addr_u + stride_u * height / 2)
        let out_v = module.HEAPU8.subarray(addr_v, addr_v + stride_v * height / 2)
        // debugger
        //render
        let obj = {
            buf_y: new Uint8Array(out_y), //webGL 使用的格式
            buf_u: new Uint8Array(out_u),
            buf_v: new Uint8Array(out_v),
            stride_y,
            stride_u,
            stride_v,
            width,
            height,
            pts
        }
        console.log(obj)
    }
    let videoCallback = module.addFunction(decodeCallback, 'viiiiiiiii');

    function videoInitCallback(width:any,
                        height:any, 
                        durationMs:any,
                        fps:any) {
        console.log(width, height, durationMs, fps);
    }
    let initCallback = module.addFunction(videoInitCallback, 'viiif');

    //从js -> wasm
    fetch('test.mp4').then(res => {
        res.arrayBuffer().then(data => {
            let datap = module._malloc(data.byteLength)
            let heap = new Uint8Array(module.HEAPU8.buffer, datap, data.byteLength)
            heap.set(new Uint8Array(data))
            //只需要调用一次
            let handle = module._ffwasm_decode_open(datap, data.byteLength, initCallback);
            if (handle < 0) {
                console.log("_ffwasm_decode_open error");
                return;
            }
            
            for (var i = 0; i < 1000; i++) {
                let ret = module._ffwasm_decode_frame(handle, i * 30, videoCallback);
                if (ret < 0) {
                    console.log("_ffwasm_decode_frame error ret " + ret + " i " + i);
                    break;
                }
                console.log("index " + i);
            }

            //释放c内存
            module._ffwasm_decode_free(handle);
            //释放外部内存
            module._free(datap);

        })
    })
    

    console.log('success');
    
})

/*
 let length = 10;
    //从js -> wasm
    let data = module._malloc(length)
    let heap = new Uint8Array(module.HEAPU8.buffer, data, length)
    heap.set([97])
    //只需要调用一次
    module._ffwasm_decode_open(data, length, videoCallback);
    module._free(data);
    */