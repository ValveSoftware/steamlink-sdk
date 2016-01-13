function function2InScript(a)
{
    logger("return function2InScript");
    root.result = a;
}

function functionInScript(a, b)
{
    logger("enter functionInScript");
    var names = ["Clark Kent", "Peter Parker", "Bruce Wayne"];
    var aliases = ["Superman", "Spiderman", "Batman"];
    var details = {
        category: "Superheroes",
        names: names,
        aliases: aliases
    };
    function2InScript(a + b);
    logger("return functionInScript");
    return details;
}

function logger(msg)
{
    //console.log(msg);
    return true;
}

//DO NOT CHANGE CODE ABOVE THIS LINE
