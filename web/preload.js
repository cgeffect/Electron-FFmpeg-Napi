'use strict';

const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('nativeVideo', {
  pickFile: () => ipcRenderer.invoke('video:pickFile'),
  open: (filePath) => ipcRenderer.invoke('video:open', filePath),
  decode: (ptsMs) => ipcRenderer.invoke('video:decode', ptsMs),
  seek: (ptsMs) => ipcRenderer.invoke('video:seek', ptsMs),
  holdSeek: (seek) => ipcRenderer.invoke('video:holdSeek', seek),
  close: () => ipcRenderer.invoke('video:close'),
});
