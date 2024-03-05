$(document).ready(() => {
    $("#login-form span").click(() => {
        $("#login-form span").toggleClass("fa-eye fa-eye-slash");
        const iconElem = $("#login-form span")
        const inputElem = $("#password");
        if (iconElem.attr('class') == 'fa fa-eye') {
            inputElem.attr('type', 'password');
        }
        if (iconElem.attr('class') == 'fa fa-eye-slash') {
            inputElem.attr('type', 'text');
        }
    })
})

function logout() {
    const form = document.createElement('form');
    form.method = 'POST';
    form.action = 'logout';
    document.body.appendChild(form);
    form.submit();
}
