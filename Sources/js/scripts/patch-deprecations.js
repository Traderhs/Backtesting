// 간단한 postinstall 패치 스크립트
const fs = require('fs');
const path = require('path');

const projectRoot = path.join(__dirname, '..');
const spawnCommandPath = path.join(projectRoot, 'node_modules', 'spawn-command', 'lib', 'spawn-command.js');

function replaceIfExists(filePath, from, to) {
  try {
    if (!fs.existsSync(filePath)) {
        return false;
    }

    const content = fs.readFileSync(filePath, 'utf8');

    if (content.indexOf(from) === -1) {
        return false;
    }

    const newContent = content.split(from).join(to);

    fs.writeFileSync(filePath, newContent, 'utf8');
    return true;
  } catch (e) {
    return false;
  }
}

// spawn-command: options = util._extend({}, options);
replaceIfExists(spawnCommandPath, "util._extend({}, options)", "Object.assign({}, options)");
