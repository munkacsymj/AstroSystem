#!/usr/bin/python3
import math

HWHM = 2.25 # pixels
Gain = 1.3                     # e-/ADU
ApertureRadius = 5
ReadNoise = 7.5                # e-/pixel (noise)
DarkCurrent = 0.00326*120      # e-/pixel (current)
SkyglowCurrent = 3.766*120     # e-/pixel (current)
TotalFlux = 69937.5            # integrated over entire exposure & PSF
StackSize = 4

DarkNoise = math.sqrt(DarkCurrent)
SkyglowNoise = math.sqrt(SkyglowCurrent)
CameraNoise = math.sqrt(ReadNoise**2 + DarkNoise**2 + SkyglowNoise**2)


Cfwhm = math.sqrt(-(HWHM**2)/(2*math.log(0.5)))
print('Cfwhm = ', Cfwhm)

def g_erf(x):
    return math.erf(x/(math.sqrt(2)*Cfwhm))

def g_int(a, b):
    return 0.5 * (g_erf(b) - g_erf(a))

integral = 0.0
sum_flux = 0.0
sum_noise_sq = 0.0
sum_shot_noise_sq = 0.0
peak_flux = 0.0
total_noise = 0.0

# pixel tuple = (e_flux, shot_noise_e, total_noise_e)
pixel_data = {}

print('Peak-ADU-to-total-flux ratio = ',
      1.0/(2.0*math.pi*Cfwhm*Cfwhm))

for row in range(-ApertureRadius, ApertureRadius, 1):
    slice = g_int(row, row+1)
    pixel_data[row] = {}
    #print('row = [', row, ', ', row+1, '] and slice integral = ', slice)

    for col in range(-ApertureRadius, ApertureRadius, 1):
        pixel = g_int(col, col+1) * slice * TotalFlux
        peak_flux = max(pixel, peak_flux)
        radius_sq = (row+0.5)*(row+0.5) + (col+0.5)*(col+0.5)
        InAperture = (radius_sq <= ApertureRadius)
        pixel_shot_noise = math.sqrt(pixel)
        pixel_total_noise = math.sqrt(pixel_shot_noise**2 + CameraNoise**2)
        
        pixel_data[row][col] = (pixel, pixel_shot_noise, pixel_total_noise)

def MeasureAndStack(stacksize):
    global sum_noise_sq, sum_flux, sum_shot_noise_sq, total_noise
    sum_noise_sq = 0.0
    sum_flux = 0.0
    sum_shot_noise_sq = 0.0
    num_in_aperture = 0
    for row in range(-ApertureRadius, ApertureRadius, 1):
        for col in range(-ApertureRadius, ApertureRadius, 1):
            radius_sq = (row+0.5)*(row+0.5) + (col+0.5)*(col+0.5)
            InAperture = (radius_sq <= ApertureRadius*ApertureRadius)
            if InAperture:
                (flux, shot_n, total_n) = pixel_data[row][col]
                total_n /= math.sqrt(stacksize)
                sum_flux += flux
                sum_noise_sq += (total_n*total_n)
                sum_shot_noise_sq += (shot_n*shot_n)/stacksize
                num_in_aperture += 1
    total_noise = math.sqrt(sum_noise_sq)
    print(num_in_aperture, ' pixels within the aperture.')
    

MeasureAndStack(1)
print('Peak flux e- = ', peak_flux)
print('Peak flux ADU = ', peak_flux/Gain)
print('Total electron flux = ', sum_flux)
print('Total shot noise e- = ', math.sqrt(sum_shot_noise_sq))
print('Total noise e- = ', total_noise)
print('Photometric accuracy = ', total_noise/sum_flux)

def PrintPixel(row, col):
    (flux, shot_n, total_n) = pixel_data[row][col]
    print('Pixel[',row,',',col,']: flux = ', flux, ', shot_noise = ',
          shot_n, ', total_noise = ', total_n)
    
PrintPixel(0,0)
#PrintPixel(12,12)

MeasureAndStack(StackSize)
print('Stack of ', StackSize, ': ')
print('Photometric accuracy = ', total_noise/sum_flux)

print('Cfwhm = ', Cfwhm)




