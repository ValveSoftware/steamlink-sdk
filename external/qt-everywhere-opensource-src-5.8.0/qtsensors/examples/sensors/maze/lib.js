//global variables
var labyrinth = null;
var dimension = 24;
var cellDimension = 13;
var won;
var objectArray = null;
var sec = 0.0

//Allocate labyrinth arrays and create labyrinth and way reflected in the labyrinth array
function createLabyrinth()
{
    won = false;
    //create the labyrinth matrix
    labyrinth = null;
    labyrinth = new Array(dimension);
    for (var x = 0; x < dimension; x++ ){
        labyrinth[x] = new Array(dimension);
        for (var y = 0; y < dimension; y++ ){
            labyrinth[x][y] = 0;
        }
    }
    createWay();
    createLab();
}

//Create a way where the mouse can reach the cheese
function createWay()
{
    //Create rnd way to have at least one solution
    //A way square is marked as a 2 in the labyrinth array
    var x = 0;
    var y = 0;
    var ox = x;
    var oy = y;
    labyrinth[0][0] = 2;
    while (x < dimension && y < dimension){
        var rnd = Math.floor(Math.random()*5);
        if (Math.floor(Math.random()*2) == 1){
            if (rnd == 0) x--;
            if (rnd >= 1) x++;
            if (x < 0) x++;
            if (x >= dimension){
                x = ox;
                break;
            }
        }
        else {
            if (rnd == 0) y--;
            if (rnd >= 1) y++;
            if (y < 0) y++;
            if (y >= dimension){
                y = oy;
                break;
            }
        }

        /*avoid to have [2]2|
                        |2|2|*/
        if (x < (dimension - 1) && y < (dimension - 1)){
            if (labyrinth[x + 1][y] == 2
                && labyrinth[x][y + 1] == 2
                && labyrinth[x + 1][y + 1] == 2){
                y = oy;
                x = ox;
                continue;
            }
        }
        /*avoid to have |2[2]
                        |2|2|*/
        if (x > 0 && y < (dimension - 1)){
            if (labyrinth[x - 1][y] == 2
                && labyrinth[x][y + 1] == 2
                && labyrinth[x - 1][y + 1] == 2){
                y = oy;
                x = ox;
                continue;
            }
        }
        /*avoid to have |2|2|
                        [2]2|*/
        if (x < (dimension - 1) && y > 0){
            if (labyrinth[x + 1][y] == 2
                && labyrinth[x][y - 1] == 2
                && labyrinth[x + 1][y - 1] == 2){
                y = oy;
                x = ox;
                continue;
            }
        }
        /*avoid to have |2|2|
                        |2[2]*/
        if (x > 0 && y > 0){
            if (labyrinth[x - 1][y] == 2
                && labyrinth[x][y - 1] == 2
                && labyrinth[x - 1][y - 1] == 2){
                y = oy;
                x = ox;
                continue;
            }
        }

        labyrinth[x][y] = 2;
        ox = x;
        oy = y;
    }
    //finish way
    while (x < (dimension - 1)){
        labyrinth[x][y] = 2;
        x++;
    }
    while (y < (dimension - 1)){
        labyrinth[x][y] = 2;
        y++;
    }
}

//Create the labyrinth with rnd values
function createLab()
{
    //A wall square is marked as a 1 in the labyrinth array
    //Not a wall square is marked as a 0 in the labyrinth array
    //The Cheese square is marked as a 3 in the labyrinth array
    //The start is marked as a -1 in the labyrinth array
    for (var x = 0; x < dimension; x++ ){
        var rnd = 0;
        for (var y = 0; y < dimension; y++){
            //But don't overwrite the way
            if (labyrinth[x][y] != 2){
                var rnd = Math.floor(Math.random()*2);
                var xy = 0;
                var xxy = 0;
                var xyy = 0;
                var xxyy = 0;

                if (x > 0 && y > 0){
                    xy = labyrinth[x - 1][y - 1];
                    if (xy == 2)
                        xy = 0;

                    xyy = labyrinth[x - 1][y];
                    if (xyy == 2)
                        xyy = 0;

                    xxy = labyrinth[x][y - 1];
                    if (xxy == 2)
                        xxy = 0;

                    xxyy = rnd;
                    if (xxyy == 2)
                        xxyy = 0;

                    //avoid to have to many |0|1| or |1|0| [xy  ][xxy ]
                    //                      |1[0]    |0[1] [xyy ][xxyy]
                    if (xyy == xxy && xy == xxyy && xy != xxy){
                        if (rnd == 1)
                            rnd = 0;
                        else rnd = 1;
                    }

                    //avoid to have to many |1|1| or |0|0|
                    //                      |1[1]    |0[0]
                    if (xy == xxy && xxy == xxyy && xxyy == xyy){
                        if (rnd == 1)
                            rnd = 0;
                        else rnd = 1;
                    }
                }
                else if (x == 0 && y > 0){
                    xy = labyrinth[x][y - 1];
                    if (xy == 2)
                        xy = 0;

                    xyy = rnd;
                    if (xyy == 2)
                        xyy = 0;

                    xxy = labyrinth[x + 1][y - 1];
                    if (xxy == 2)
                        xxy = 0;

                    xxyy = labyrinth[x + 1][y];
                    if (xxyy == 2)
                        xxyy = 0;

                    //avoid to have to many |1|1| or |0|0|
                    //                      |1[1]    |0[0]
                    if (xy == xxy && xxy == xxyy && xxyy == xyy){
                        if (rnd == 1)
                            rnd = 0;
                        else rnd = 1;
                    }

                    //avoid to have to many |0|1| or |1|0| [xy  ][xxy ]
                    //                      |1[0]    |0[1] [xyy ][xxyy]
                    if (xyy == xxy && xy == xxyy && xy != xxy){
                        if (rnd == 1)
                            rnd = 0;
                        else rnd = 1;
                    }
                }
                labyrinth[x][y] = rnd;
            }

        }
    }
    //set start and end
    labyrinth[0][0] = -1;
    labyrinth[0][1] = 0;
    labyrinth[1][0] = 0;
    labyrinth[1][1] = 0;

    labyrinth[dimension - 2][dimension - 2] = 0;
    labyrinth[dimension - 2][dimension - 1] = 0;
    labyrinth[dimension - 1][dimension - 2] = 0;
    labyrinth[dimension - 1][dimension - 1] = 3;
}

//Function that checks if the mouse can be moved in x and y
function canMove(x, y)
{
    //Check if movement is allowed
    var xcenter = x + (cellDimension / 2);
    var ycenter = y + (cellDimension / 2);
    //try to get the index
    var idx = Math.floor(xcenter / cellDimension);
    var idy = Math.floor(ycenter / cellDimension);
    var dx = xcenter - (idx * cellDimension + ( cellDimension / 2 ));
    var dy = ycenter - (idy * cellDimension + ( cellDimension / 2 ));

    if (dx > 0){
        if (labyrinth[idx][idy] == 1)
            return false;
    }
    if (dx < 0){
        if (labyrinth[idx][idy] == 1)
            return false;
    }
    if (dy > 0){
        if (labyrinth[idx][idy] == 1)
            return false;
    }
    if (dy < 0){
        if (labyrinth[idx][idy] == 1)
            return false;
    }
    //check if won
    if (idx == (dimension - 1) && idy == (dimension - 1))
        won = true;
    return true;
}

//Function that prints out the labyrith array values in the console
function printLab()
{
    //for debug purposes print out lab n console
    var iy = 0;
    for (var y = 0; y < dimension; y++ ){
        var line = "";
        for (var x = 0; x < dimension; x++ ){
            line += labyrinth[x][y];
        }
        console.log(line);
    }
}
