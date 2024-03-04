$(document).ready(() => {
    $("input[type='button']").click(() => {
        // console.log($("input[type='password']").val())
    })
    
    $("#login-form span").click(() => {
        $("#login-form span").toggleClass("fa-eye fa-eye-slash");
        const iconElem = $("#login-form span")
        const inputElem = $("#password");
        if(iconElem.attr('class') == 'fa fa-eye') {
            inputElem.attr('type', 'password');
        }
        if(iconElem.attr('class') == 'fa fa-eye-slash') {
            inputElem.attr('type', 'text');
        }
    })

    $('.blur,#show').hover(
      function() {
        // Mouse over
        const $input = $('ul li input');
        $input.attr('type', 'text');
        $input.val('NewValue');
      },
      function() {
        // Mouse out
        const $input = $('ul li input');
        $input.attr('type', 'password');
      }
    );

    $('#next-btn').click(() => {
        const isChecked = $('#checkbox').is(':checked')
        console.log(isChecked)
    })
})
