const axios = require("axios")

async function main() {
    const a = await axios.get('http://93.190.139.223:8080/v1/request/')
    console.log(a.data)
}

main()