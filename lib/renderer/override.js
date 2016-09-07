'use strict'

const ipcRenderer = require('electron').ipcRenderer
const remote = require('electron').remote

// Use the dialog API to implement alert().
window.alert = function (message = '', title = '') {
  remote.dialog.showMessageBox(remote.getCurrentWindow(), {
    message: String(message),
    title: String(title),
    buttons: ['OK']
  })
}

// And the confirm().
window.confirm = function (message, title) {
  var buttons, cancelId
  if (title == null) {
    title = ''
  }
  buttons = ['OK', 'Cancel']
  cancelId = 1
  return !remote.dialog.showMessageBox(remote.getCurrentWindow(), {
    message: message,
    title: title,
    buttons: buttons,
    cancelId: cancelId
  })
}

// But we do not support prompt().
window.prompt = function () {
  throw new Error('prompt() is and will not be supported.')
}

if (process.openerId != null) {
  window.opener = BrowserWindowProxy.getOrCreate(process.openerId)
}

// Forward history operations to browser.
var sendHistoryOperation = function (...args) {
  return ipcRenderer.send.apply(ipcRenderer, ['ELECTRON_NAVIGATION_CONTROLLER'].concat(args))
}

var getHistoryOperation = function (...args) {
  return ipcRenderer.sendSync.apply(ipcRenderer, ['ELECTRON_SYNC_NAVIGATION_CONTROLLER'].concat(args))
}

window.history.back = function () {
  return sendHistoryOperation('goBack')
}

window.history.forward = function () {
  return sendHistoryOperation('goForward')
}

window.history.go = function (offset) {
  return sendHistoryOperation('goToOffset', offset)
}

Object.defineProperty(window.history, 'length', {
  get: function () {
    return getHistoryOperation('length')
  }
})

// The initial visibilityState.
let cachedVisibilityState = process.argv.includes('--hidden-page') ? 'hidden' : 'visible'

// Subscribe to visibilityState changes.
ipcRenderer.on('ELECTRON_RENDERER_WINDOW_VISIBILITY_CHANGE', function (event, visibilityState) {
  if (cachedVisibilityState !== visibilityState) {
    cachedVisibilityState = visibilityState
    document.dispatchEvent(new Event('visibilitychange'))
  }
})

// Make document.hidden and document.visibilityState return the correct value.
Object.defineProperty(document, 'hidden', {
  get: function () {
    return cachedVisibilityState !== 'visible'
  }
})

Object.defineProperty(document, 'visibilityState', {
  get: function () {
    return cachedVisibilityState
  }
})
