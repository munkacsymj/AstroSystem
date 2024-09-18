/T=/ { ltime = substr($1, 3);
}

/MV=/ { print ltime, $0 }
