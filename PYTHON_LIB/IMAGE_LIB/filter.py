# The types: 'CATALOG', 'AAVSO', 'ASTRO_DB', 'CANONICAL'
filter_dictionary = [
    ('PV', 'V', 'CATALOG'),
    ('PB', 'B', 'CATALOG'),
    ('PU', 'U', 'CATALOG'),
    ('PR', 'R', 'CATALOG'),
    ('PI', 'I', 'CATALOG'),
    ('PJ', 'J', 'CATALOG'),
    ('PH', 'H', 'CATALOG'),
    ('PK', 'K', 'CATALOG'),
    ('V' , 'V', 'AAVSO'),
    ('B' , 'B', 'AAVSO'),
    ('U' , 'U', 'AAVSO'),
    ('R' , 'R', 'AAVSO'),
    ('I' , 'I', 'AAVSO'),
    ('J' , 'J', 'AAVSO'),
    ('H' , 'H', 'AAVSO'),
    ('K' , 'K', 'AAVSO'),
    ('Vc', 'V', 'ASTRO_DB'),
    ('Bc', 'B', 'ASTRO_DB'),
    ('U' , 'U', 'ASTRO_DB'),
    ('Rc', 'R', 'ASTRO_DB'),
    ('Ic', 'I', 'ASTRO_DB'),
    ('J' , 'J', 'ASTRO_DB'),
    ('H' , 'H', 'ASTRO_DB'),
    ('K' , 'K', 'ASTRO_DB')]

to_canonical = {}
to_type = {}
canonical_set = set()
for (sp,canonical,type) in filter_dictionary:
    canonical_set.add(canonical)
    if sp not in to_canonical:
        to_canonical[sp] = canonical

def ToCanonical(random_name):
    return to_canonical[random_name]

def lookup(canonical, type):
    return next(x[0] for x in filter_dictionary if x[1] == canonical and x[2] == type)

for c in canonical_set:
    to_type[c] = { 'CATALOG': lookup(c,'CATALOG'),
                   'AAVSO' : lookup(c, 'AAVSO'),
                   'ASTRO_DB' : lookup(c, 'ASTRO_DB'),
                   'CANONICAL' : c }

def FilterSynonyms(filtername, type):
    return to_type[to_canonical[filtername]][type]

################################
# Color Transformations
################################
color_index_coefficients = {} # indices are 'bv', 'br', 'bi', 'vr', ...
filter_coefficients = {} # indices are 'b_bv', 'b_br', ...

def ReadCoefficientsFile():
    global color_index_coefficients, filter_coefficients
    with open('/home/ASTRO/CURRENT_DATA/transforms.ini', 'r') as fp:
        section = None
        line = fp.readline()
        while line:
            if line[0] == '[':
                if '[Setup]' in line:
                    section = 'setup'
                if '[Coefficients]' in line:
                    section = 'coefficients'
                if '[Error]' in line:
                    section = 'error'
                if '[R Squared Values]' in line:
                    section = 'R2'
            elif section == 'coefficients':
                words = line.split('=')
                var_name = words[0][1:].upper()
                value = float(words[1].strip())
                if '_' in var_name:
                    filter_coefficients[var_name] = value
                else:
                    color_index_coefficients[var_name] = value
            line = fp.readline()
    #print("Using transformation coefficients:")
    #print(color_index_coefficients)
    #print(filter_coefficients)
        
ReadCoefficientsFile()

transform_preferences = {
    'V' : ['V_BV', 'V_VR', 'V_VI'],
    'R' : ['R_VR', 'R_RI', 'R_VI'],
    'B' : ['B_BV', 'B_BR', 'B_BI'],
    'I' : ['I_RI', 'I_VI']
    }

# Returns value that should be *added* to the standard magnitude
def CatColorTransform(this_filter, # relevant filter
                      ref_color,   # dictionary indexed by filter that defines the ref color
                      xform_info,  # dictionary
                      cat_entry):  # reference to gstar catalog entry
    this_filter = to_canonical[this_filter]
    xform_to_use = None
    if this_filter not in transform_preferences:
        print("Warning: CatColorTransform can't handle filter ", this_filter)
        return 0.0
    for x in transform_preferences[this_filter]:
        depend0 = to_canonical[x[2]]
        depend1 = to_canonical[x[3]]
        if (depend0 in cat_entry.ref_mag and
            depend1 in cat_entry.ref_mag):
            xform_to_use = x
            break

    if xform_to_use == None:
        print("CatColorTransform: no transform?")
        print("... filter = ", this_filter, ", cat_entry = ", cat_entry.ref_mag)
        return 0.0

    this_coefficient = filter_coefficients[xform_to_use]
    this_color = (cat_entry.ref_mag[depend0][0] -
                  cat_entry.ref_mag[depend1][0])
    delta_color = (this_color -
                   (ref_color[depend0] - ref_color[depend1]))
    adjust = delta_color*this_coefficient
    if xform_info != None:
        xform_info['DELTA_'+xform_to_use[-2:]] = delta_color
        xform_info['X_ADJ'] = adjust
        xform_info['T'+xform_to_use.lower()] = this_coefficient
    return adjust


def ImgColorTransform(bvri_star,
                      ref_color,
                      cat_entry):
    # If there's only one measurement, special handling: transform using catalog color
    if len(bvri_star.sources) == 1:
        if len(bvri_star.g_star.ref_mag) > 1:
            mstar = next(iter(bvri_star.sources.values())) # value of first item
            xform_adj = CatColorTransform(mstar.filter,
                                          ref_color,
                                          mstar.transform_info,
                                          bvri_star.g_star)
            if xform_adj != None:
                mstar.transformed_mag = mstar.mag + xform_adj
                mstar.transformed_ucty = mstar.uncertainty
                mstar.xform_method = "catalog_color"
        return

    # There are multiple measurements; do each one in turn
    # iterate 3 times to achieve convergence
    success = {}
    for filter in bvri_star.sources:
        success[filter] = False
        bvri_star.sources[filter].transformed_mag = bvri_star.sources[filter].mag
        
    for count in [1,2,3]:
        for (this_filter,m_star) in bvri_star.sources.items():
            xform_to_use = None
            if this_filter not in transform_preferences:
                print("Warning: ImgColorTransform can't handle filter ", this_filter)
                continue # next filter for this star
            for x in transform_preferences[this_filter]:
                depend0 = to_canonical[x[2]]
                depend1 = to_canonical[x[3]]
                if (depend0 in bvri_star.sources and
                    depend1 in bvri_star.sources):
                    xform_to_use = x
                    break

            print("count = ", count, ", filter = ", this_filter, ", xform = ", xform_to_use)
            if xform_to_use != None:
                success[this_filter]=True
                this_coefficient = filter_coefficients[xform_to_use]
                this_color = (bvri_star.sources[depend0].transformed_mag -
                              bvri_star.sources[depend1].transformed_mag)
                delta_color = (this_color -
                               (ref_color[depend0] - ref_color[depend1]))
                adjust = delta_color*this_coefficient
                xform_info = m_star.transform_info
                xform_info['DELTA_'+xform_to_use[-2:]] = delta_color
                xform_info['X_ADJ'] = adjust
                xform_info['T'+xform_to_use.lower()] = this_coefficient
                xform_info['UNCTY_SRC'] = m_star.uncty_source
                m_star.transformed_mag = m_star.mag + adjust
                m_star.transformed_uncty = m_star.uncertainty
                m_star.xform_method = xform_to_use[-2:]
                print("color=", xform_to_use[-2:], " (color)_star = ", this_color, ", (color)_ref = ",
                      ref_color[depend0]-ref_color[depend1])
                print("untransformed = ", m_star.mag, ", became ", m_star.transformed_mag)

    for (this_filter,m_star) in bvri_star.sources.items():
        print("xform ", this_filter, " from ",
              m_star.mag, " to ", m_star.transformed_mag)



        
    
