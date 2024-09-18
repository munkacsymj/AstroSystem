BEGIN   { print "T="TIME, "F="SOURCE_FILE }
        { sub("#.*", "") }
/Total err/    { next }
NF==5||NF==4   { print "S="$1, "MV="$2, "E="$3, "N="$4 }
NF==8||NF==7   { print "S="$1, "MV="$4, "E="$6, "N="$7 }
