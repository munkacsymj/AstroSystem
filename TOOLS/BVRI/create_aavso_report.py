#!/usr/bin/python3

import sys, getopt
import os
import report_db

def usage():
    print('usage: create_aavso_report.py -i aavso_report.db -o output.txt')
    sys.exit(2)

def main(argv):
    mode = None
    db_filename = None
    output_filename = None

    try:
        opts, remainder_args = getopt.getopt(argv,"i:o:")
    except getopt.GetoptError:
        usage()

    for opt, arg in opts:
        if opt == '-i':
            db_filename = arg
        elif opt == '-o':
            output_filename = arg

    if db_filename == None or output_filename == None:
        usage()

    r = report_db.ReportDB(db_filename)
    all_data = r.get_all_lines_and_annotations()

    conflicts = [x for x in all_data if 'c' in x[0]]
    if len(conflicts) > 0:
        print("ERROR: create_aavso_report: conflicts are present.")
        print("Cannot generate aavso_report.")
    else:
        bvri_command = 'bvri_report -h -o /tmp/bvri_header.txt' 
        os.system(bvri_command)
        fp_header = open('/tmp/bvri_header.txt', 'r')
        header_lines = fp_header.readlines()
        fp_header.close()

        fp_out = open(output_filename, 'w')
        for line in header_lines:
            fp_out.write(line)

        record_count = 0
        for x in all_data:
            # if record hasn't been deleted...
            if not 'd' in x[0]:
                fp_out.write(x[1])
                fp_out.write('\n')
                record_count += 1
        fp_out.close()
        print(record_count, ' lines written.')

if __name__ == "__main__":
    main(sys.argv[1:])
