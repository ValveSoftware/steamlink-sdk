var savedReference;

function startup()
{
    savedReference = object.rect;
    console.log("Test: " + savedReference.x);
}

function afterDelete()
{
    console.log("Test: " + savedReference.x);
}

