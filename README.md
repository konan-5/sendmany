# sendmany
Pure C code for general interface to qubic network and specifically sendmany SC

gcc -DTESTNET main.c -o smany # for testnet
gcc main.c -o smany # for mainnet

run ./smany to get usage details

## Using CLI UI
### Installation
```
cd app
npm install
```

### build wasm.c
```
make build
```

### Run UI
```
npm start
```
- Open this link
http://localhost:3000/test
