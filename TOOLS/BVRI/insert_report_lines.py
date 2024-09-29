#!/usr/bin/python3

import sys, getopt
import report_db

def usage():
    print('usage: insert_report_lines.py -a|m [-d] [-f filename] -i aavso_report.db -L "text line"')
    sys.exit(2)

def main(argv):
    mode = None
    input_filename = None
    do_delete = False
    input_text_line = None
    db_filename = None
    input_text = None

    try:
        opts, remainder_args = getopt.getopt(argv,"amdf:i:L:")
    except getopt.GetoptError:
        usage()

    for opt, arg in opts:
        if opt == '-a':
            mode = "a"
        elif opt == '-m':
            mode = "m"
        elif opt == '-f':
            input_filename = arg
        elif opt == '-d':
            do_delete = True
        elif opt == '-i':
            db_filename = arg
        elif opt == '-L':
            input_text_line = arg

    if db_filename == None and input_text_line == None:
        usage()
    if mode == None and do_delete == False:
        usage()

    r = report_db.ReportDB(db_filename)
    if input_text_line != None:
        input_text = [input_text_line]
    elif input_filename != None:
        f = open(input_filename)
        input_text = [x for x in f.readlines() if x[0] != '#']
    else:
        usage()

    if do_delete:
        r.delete_lines(input_text)
    else:
        r.insert_lines(input_text, mode)
    r.save()

if __name__ == "__main__":
    main(sys.argv[1:])
