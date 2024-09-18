#!/usr/bin/wish

package require Plotchart
canvas .c -background white -width 400 -height 200

set low_focus_limit 1500.0
set high_focus_limit 1510.0

proc TrendlineParamA { trendline } {
    set A [lindex $trendline 0]
    return $A
}
proc TrendlineParamB { trendline } {
    set B [lindex $trendline 1]
    return $B
}
proc TrendlineParamR { trendline } {
    set R [lindex $trendline 2]
    return $R
}

proc AddDataPoints { plot_canvas points } {
    foreach pair  $points {
	set x [lindex $pair 0]
	set y [lindex $pair 1]
	$plot_canvas dot series1 $x $y 0.0
    }
}

proc AddTrendLine { plot_canvas trendline } {
    # trendline y = a * sqrt(a + (x-R)^2/b^2)
    set numpoints 40
    global high_focus_limit
    global low_focus_limit
    
    set p_interval [ expr { ( $high_focus_limit - $low_focus_limit ) / $numpoints } ]
    for {set i $low_focus_limit} {$i<=$high_focus_limit} {set i [expr $i + $p_interval] } {
	set temp1 [expr { ($i - [TrendlineParamR $trendline])**2 } ]
	#puts "temp1 = $temp1"
	set val [expr { [TrendlineParamA $trendline] * sqrt(1.0+($i-[TrendlineParamR $trendline])**2/[TrendlineParamB $trendline]**2) } ]
	#puts "Plotting $i $val"
	$plot_canvas plot series2 $i $val
    }
}

proc RedrawWholeGraph { points trend_line } {
    global s
    global x_axis_info
    global y_axis_info
    global plot_canvas
    
    $plot_canvas deletedata
    
    #::Plotchart::clearcanvas $plot_canvas
    #set plot_canvas [::Plotchart::createXYPlot .c $x_axis_info $y_axis_info]
    #$plot_canvas dotconfig series1 -colour red -scalebyvalue off
    
    AddDataPoints $plot_canvas $points
    if { [llength $trend_line] } {
	AddTrendLine $plot_canvas $trend_line
    }
}
    
proc ReadFocusData { } {
    set pipe [open "/tmp/focus.data.pipe" "r"]
    set AllPoints {}
    set TrendLine {}
    global low_focus_limit
    global high_focus_limit
    global plot_canvas
    
    set terminate false
    while {$terminate == false && ! [eof $pipe] } {
	set thisline [gets $pipe]
	puts "fetched '$thisline' from pipe."
	set words [regexp -all -inline {\S+} $thisline]
	set command [lindex $words 0]
	puts "executing command: $command"
	switch $command {
	    "point" {
		set x [lindex $words 1]
		set y [lindex $words 2]
		lappend AllPoints [list $x $y]
		if { $x >= $low_focus_limit && $x <= $high_focus_limit } {
		    puts "simple plot update at ($x, $y)"
		    $plot_canvas dot series1 $x $y 0.0
		} else {
		    if { $x < $low_focus_limit } {
			set low_focus_limit $x
		    }
		    if { $x > $high_focus_limit } {
			set high_focus_limit $x
		    }
		    puts "reset axes to $low_focus_limit, $high_focus_limit"
		    set x_axis_info [list $low_focus_limit $high_focus_limit 100.0 ]
		    $plot_canvas xconfig -scale $x_axis_info
		    RedrawWholeGraph $AllPoints $TrendLine
		}
	    }
	    "curve" {
		set a [lindex $words 1]
		set b [lindex $words 2]
		set r [lindex $words 3]

		set TrendLine [list $a $b $r]
		RedrawWholeGraph $AllPoints $TrendLine
	    }
	    "quit" {
		set terminate true
	    }
	    default {
		puts "invalid control word received on pipe: $words"
	    }
	}
	update
    }
    puts "Finished with main loop."
}
		
pack .c -fill both

#set TrendLine [list 0.5 [expr { 64.0 * 0.5 }] 1200.0 ]
#puts "trendline = [TrendlineParamA $TrendLine] [TrendlineParamB $TrendLine] [TrendlineParamR $TrendLine]"

set low_focus_limit 900.0
set high_focus_limit 1460.0

set x_axis_info [list $low_focus_limit $high_focus_limit 100.0 ]
set y_axis_info [list 0.0 40.0 5.0 ]

set plot_canvas [::Plotchart::createXYPlot .c $x_axis_info $y_axis_info]
$plot_canvas dotconfig series1 -colour red -scalebyvalue off

ReadFocusData
