#
# Format of a line in the database:
#
# $xxx$AX AND,2458902.6006,13.361,0.006,B,YES,STD,000-BBF-079,21.874,000-BFT-670,22.761,1.707,32,16940BA,BMAGINS=21.733|BERR=0.006|CREFMAG=13.488|b-v=1.567|Tb_bv=0.015|CHKERRRMS=0.058|NUMCHKSTARS=11|NUMCOMPSTARS=1
#
# The string "xxx" has either two or three characters:
#   Combinations of one or more of the following four characters are
# found in the flag string:
#  - 'c': conflict is present
#  - 'd': line has been deleted
#  - 'm': line was manually entered
#  - 'a': line was algorithmically created
#
# Format of a key string: "AX-AND/B"
#

def extract_key(text_line):
    words = text_line.split(',')
    if len(words) < 14:
        print("extract_key: Bad input line:")
        print(text_line)
    else:
        return words[0].replace(' ','-')+words[4]

class ReportEntry:
    def __init__(self, flags, text):
        self.flags = flags
        self.key = extract_key(text)
        self.text = text.replace('\n', '')

    def file_form(self):
        return '$'+self.flags+'$'+self.text+'\n'

class ReportDB:
    def __init__(self, filename):
        self._filename = filename

        try:
            fp = open(filename, 'r')
            self._all_lines = self.extract_from_file(fp)
            fp.close()
        except IOError:
            print("Warning: database file doesn't already exist.")
            self._all_lines = []

    def refresh_from_file(self):
        try:
            fp = open(self._filename, 'r')
            self._all_lines = self.extract_from_file(fp)
            fp.close()
        except IOError:
            print("Warning: database file doesn't already exist.")
            self._all_lines = []
        
    def delete_lines(self, list_of_lines):
        delete_count = 0
        for one_line in list_of_lines:
            my_key = extract_key(one_line)
            last_match_index = None
            for index,match in enumerate(self._all_lines):
                if my_key != match.key: continue

                # line has been found!
                delete_count += 1
                match.flags = match.flags.replace('c','') # turn off conflict
                if 'd' in match.flags:
                    match.flags = match.flags.replace('d','')
                else:
                    match.flags += 'd'

        if delete_count == 1:
            print("1 entry deleted")
        else:
            print(delete_count, " entries deleted.")

    def insert_lines(self, list_of_lines, mode_character):
        for one_line in list_of_lines:
            my_key = extract_key(one_line)
            last_match_index = None
            entered = False
            for index,match in enumerate(self._all_lines):
                if my_key != match.key: continue

                last_match_index = index
                match.text = one_line
                entered = True
                if mode_character == 'm':
                    # Manual: under all conditions, becomes the entry of
                    # record and supercedes any "auto" entry

                    if 'd' in match.flags:
                        # attempting to mod a deleted line
                        match.flags = 'mdc' # creates conflict
                    else:
                        match.flags = 'm'
                else:
                    # automatic line being inserted
                    if 'd' in match.flags:
                        # automatic update of deleted line: creates conflict
                        match.flags = 'adc'
                    else:
                        if 'm' in match.flags:
                            # Creates a conflict
                            match.flags = 'ac'
            if not entered:
                new_entry = ReportEntry(mode_character, one_line)
                if last_match_index == None:
                    self._all_lines.append(new_entry)
                else:
                    self._all_lines.insert(last_match_index, new_entry)

    def save(self):
        fp = open(self._filename, 'w')
        for r in self._all_lines:
            fp.write(r.file_form())
        fp.close()

#    def set_do_not_use(self, key, mode_character):
#        num_changed = 0
#        for object in self._all_lines:
#            if search_key == object.key and mode_character in object.flags:
#                object.flags[1] = 'o'
#                num_changed += 1
#        if num_changed != 1:
#            print("set_do_not_use: num_changed = " + str(num_changed))
            
    def get_all_lines_and_annotations(self):
        return [[x.flags, x.text] for x in self._all_lines]

    def get_all_lines_with_annotations(self, search_key=None):
        if search_key == None:
            return [x.file_form() for x in self._all_lines]
        else:
            return [x.file_form() for x in self._all_lines if
                    search_key == x.key]
        
    # No annotations included
    def get_all_lines_to_submit(self):
        return [ x.text for x in self._all_lines if 'x' in x.flags ]

    #################################
    #    PRIVATE METHODS
    #################################
    def extract_from_file(self, fp):
        result = []
        for r in fp.readlines():
            words = r.split('$')
            if len(words) != 3:
                print("Bad line in file?? ")
                print(r)
            else:
                result.append(ReportEntry(words[1], words[2]))
        return result
        
