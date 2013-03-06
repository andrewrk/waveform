{
  "targets": [
    {
      "target_name": "waveform",
      "type": "executable",
      "cflags": [
        "-O3"
      ],
      "libraries": [
        "-lsox",
        "-lz",
        "-lpng"
      ],
      "sources": [
        "main.c"
      ]
    }
  ]
}
