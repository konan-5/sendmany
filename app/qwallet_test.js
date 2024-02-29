const createModule = require('./public/a.out.js');

const Module = createModule()

const message = "v1request"

const result = Module.ccall("qwallet", 'string', ['string'], [message])
console.log(result)
process.exit()
