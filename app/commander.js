const createModule = require('./public/a.out.js');

const args = process.argv.splice(2)

function createStringArray(n) {
    return new Array(n).fill('string');
}

const stringArray = createStringArray(args.length);

const Module = createModule()
const result = Module.ccall("qwallet", 'string', stringArray, args)
// const result_json = JSON.parse(result)
console.log(`_^_${result}_^_`)
