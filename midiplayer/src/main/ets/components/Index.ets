import miden from 'libmidiplayer.so'
import resourceManager from '@ohos.resourceManager'

export class NotePlayer {
  private notesMap: Map<number, number> = new Map()

  async init() {
    miden.initEngine()
  }

  async destroy() {
    this.notesMap.forEach((timerId: number, noteId: number) => {
      clearTimeout(timerId)
    })
    this.notesMap.clear()

    miden.destroyEngine()
  }

  async loadSF(resMan: resourceManager.ResourceManager, path: string) {
    try
    {
      const data = await resMan.getRawFileContent(path)
      let ok = miden.loadSF(data.buffer)
      this.startPlayback()
      return ok
    } catch (error)
    {
      console.error('无法加载sf2', JSON.stringify(error))
      return false
    }
  }

  playNote(channel: number, key: number, dur: number, vel: number = 1) {
    const noteID = this.makeNoteID(channel, key)

    if (this.notesMap.has(noteID)) {
      let timerID = this.notesMap.get(noteID)
      if (timerID !== undefined) {
        clearTimeout(timerID)
        this.notesMap.delete(noteID)
        miden.playMidi(channel, key, 0)
      }
    }

    const timerID = setTimeout(() => {
      miden.playMidi(channel, key, 0)
      this.notesMap.delete(noteID)
    }, dur)

    miden.playMidi(channel, key, vel)
    this.notesMap.set(noteID, timerID)
  }

  makeNoteID(channel: number, key: number) {
    return (channel << 7) | key
  }

  getChannel(id: number) {
    return id >> 7
  }

  getKey(id: number) {
    return id & 0x7f
  }

  startPlayback() {
    miden.startPlayback()
  }
}