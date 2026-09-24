import { readFileSync, mkdirSync, copyFileSync, writeFileSync } from 'node:fs';
import { resolve, join } from 'node:path';
import { createHash } from 'node:crypto';
const [build, core, output = 'release'] = process.argv.slice(2);
if (!build || !core) throw new Error('Usage: node scripts/package-release.mjs BUILD ESP32_CORE [OUTPUT]');
const version = readFileSync('src/FirmwareVersion.h','utf8').match(/KEIROST_FIRMWARE_VERSION "([\d.]+)"/)[1];
if (process.env.GITHUB_REF_TYPE === 'tag' && process.env.GITHUB_REF_NAME !== `v${version}`) throw new Error('Tag and firmware version must match');
mkdirSync(output,{recursive:true});
const files = [
  ['firmware.bin',join(build,'keirost-rfid-fichaje.ino.bin'),0x10000,0x140000],
  ['boot_app0.bin',join(core,'tools','partitions','boot_app0.bin'),0xe000,8192],
  ['partitions.bin',join(build,'keirost-rfid-fichaje.ino.partitions.bin'),0x8000,3072],
];
const assets = files.map(([name,path,address,max]) => {
  const bytes = readFileSync(path);
  if (bytes.length < 1024 || bytes.length > max || (name !== 'firmware.bin' && bytes.length !== max)) throw new Error(`Invalid size: ${name}`);
  if (name === 'firmware.bin' && (bytes[0] !== 0xe9 || bytes.readUInt16LE(12) !== 0)) throw new Error('Only ESP32 firmware is supported');
  if (name === 'partitions.bin') {
    for (const [entry,offset,size] of [[0,0x9000,0x5000],[1,0xe000,0x2000],[2,0x10000,0x140000],[3,0x150000,0x140000]]) {
      if (bytes.readUInt32LE(entry*32+4) !== offset || bytes.readUInt32LE(entry*32+8) !== size) throw new Error('Partition layout changed; refusing unsafe update');
    }
  }
  copyFileSync(path,resolve(output,name));
  return {name,address,size:bytes.length,sha256:createHash('sha256').update(bytes).digest('hex'),md5:createHash('md5').update(bytes).digest('hex')};
});
writeFileSync(join(output,'manifest.json'), JSON.stringify({version,board:'esp32-devkit-v1-rc522',assets},null,2)+'\n');
console.log(`Packaged firmware ${version}; partition layout checked; SHA-256 hashes generated.`);
