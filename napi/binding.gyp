{
  "targets": [
    {
      "target_name": "ffmpeg_player_napi",
      "sources": [
        "src/addon.cpp",
        "src/ffdecode.cpp",
        "src/ffrotate.cpp"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "src",
        "src/third-party/ffmpeg/deploy/include"
      ],
      "libraries": [
        "-L<(module_root_dir)/src/third-party/ffmpeg/deploy/lib",
        "-lavformat",
        "-lavcodec",
        "-lavutil",
        "-lswscale"
      ],
      "cflags_cc": [
        "-std=c++17"
      ],
      "defines": [
        "NAPI_DISABLE_CPP_EXCEPTIONS"
      ]
    }
  ]
}
