// Import the generated JavaScript file
const { program } = require('commander');
const importModule = require('./a.out.js');

// Use Commander to parse command-line arguments
// program
//     .option('--fn <type>', 'function name to call')
//     .option('--rt <type>', 'function name to call')

// program.parse(process.argv);

// const options = program.opts();

// const functionName = options.fn;
// let returnType = options.rt;

// if (!functionName) {
//     console.log('No function name provided. Please use the --name option.');
//     process.exit(1);
// }

// if (!returnType) {
//     returnType = 'string';
// }

importModule.onRuntimeInitialized = async function () {
    // Now it's safe to call the compiled functions
    const result = await importModule.ccall('qwallet', 'number', ['string', 'string'], ["a", "b"]);
    console.log(result)
    process.exit(1);
};
