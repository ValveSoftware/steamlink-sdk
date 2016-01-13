
var curVal = 0
var memory = 0
var lastOp = ""
var timer = 0

function disabled(op) {
    if (op == "." && display.text.toString().search(/\./) != -1) {
        return true
    } else if (op == squareRoot &&  display.text.toString().search(/-/) != -1) {
        return true
    } else {
        return false
    }
}

function doOperation(op) {
    if (disabled(op)) {
        return
    }

    if (op.toString().length==1 && ((op >= "0" && op <= "9") || op==".") ) {
        if (display.text.toString().length >= 14)
            return; // No arbitrary length numbers
        if (lastOp.toString().length == 1 && ((lastOp >= "0" && lastOp <= "9") || lastOp == ".") ) {
            display.text = display.text + op.toString()
        } else {
            display.text = op
        }
        lastOp = op
        return
    }
    lastOp = op

    if (display.currentOperation.text == "+") {
        display.text = Number(display.text.valueOf()) + Number(curVal.valueOf())
    } else if (display.currentOperation.text == "-") {
        display.text = Number(curVal) - Number(display.text.valueOf())
    } else if (display.currentOperation.text == multiplication) {
        display.text = Number(curVal) * Number(display.text.valueOf())
    } else if (display.currentOperation.text == division) {
        display.text = Number(Number(curVal) / Number(display.text.valueOf())).toString()
    } else if (display.currentOperation.text == "=") {
    }

    if (op == "+" || op == "-" || op == multiplication || op == division) {
        display.currentOperation.text = op
        curVal = display.text.valueOf()
        return
    }

    curVal = 0
    display.currentOperation.text = ""

    if (op == "1/x") {
        display.text = (1 / display.text.valueOf()).toString()
    } else if (op == "x^2") {
        display.text = (display.text.valueOf() * display.text.valueOf()).toString()
    } else if (op == "Abs") {
        display.text = (Math.abs(display.text.valueOf())).toString()
    } else if (op == "Int") {
        display.text = (Math.floor(display.text.valueOf())).toString()
    } else if (op == plusminus) {
        display.text = (display.text.valueOf() * -1).toString()
    } else if (op == squareRoot) {
        display.text = (Math.sqrt(display.text.valueOf())).toString()
    } else if (op == "mc") {
        memory = 0;
    } else if (op == "m+") {
        memory += display.text.valueOf()
    } else if (op == "mr") {
        display.text = memory.toString()
    } else if (op == "m-") {
        memory = display.text.valueOf()
    } else if (op == leftArrow) {
        display.text = display.text.toString().slice(0, -1)
        if (display.text.length == 0) {
            display.text = "0"
        }
    } else if (op == "Off") {
        Qt.quit();
    } else if (op == "C") {
        display.text = "0"
    } else if (op == "AC") {
        curVal = 0
        memory = 0
        lastOp = ""
        display.text ="0"
    }
}

