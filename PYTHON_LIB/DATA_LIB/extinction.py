coefficients = { 'B' : 0.271, # 0.298
                 'V' : 0.098, # 0.152
                 'R' : 0.107, # 0.160
                 'I' : 0.024 } # 0.076

def ExtinctionCorrection(airmass, ref_airmass, filter):
    return (airmass - ref_airmass)*coefficients[filter]

