<?php
    $score = $_POST["score"];
    echo "<html>";
    echo "<head><title>SameGame High Scores</title></head><body>";
    if($score > 0){#Sending in a new high score
        $name = $_POST["name"];
        $grid = $_POST["gridSize"];
        $time = $_POST["time"];
        if($name == "")
            $name = "Anonymous";
        $file = fopen("score_data.xml", "a");
        $ret = fwrite($file, "<record><score>". $score . "</score><name>"
            . $name . "</name><gridSize>" . $grid . "</gridSize><seconds>"
            . $time . "</seconds></record>\n");
        echo "Your score has been recorded. Thanks for playing!";
        if($ret == False)
            echo "<br/> There was an error though, so don't expect to see that score again.";
    }else{#Read high score list
        #Now uses XSLT to display. So just print the file. With XML cruft added.
        #Note that firefox at least won't apply the XSLT on a php file. So redirecting
        $file = fopen("scores.xml", "w");
        $ret = fwrite($file, '<?xml version="1.0" encoding="ISO-8859-1"?>' . "\n"
            . '<?xml-stylesheet type="text/xsl" href="score_style.xsl"?>' . "\n"
            . "<records>\n" . file_get_contents("score_data.xml") . "</records>\n");
        if($ret == False)
            echo "There was an internal error. Sorry.";
        else
            echo '<script type="text/javascript">window.location.replace("scores.xml")</script>';
    }
    echo "</body></html>";
?>
