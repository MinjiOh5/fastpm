# This uses nbodykit

from nbodykit.lab import Gadget1Catalog
import numpy
from argparse import ArgumentParser
import os
import shutil

ap = ArgumentParser()
ap.add_argument('source', help='FastPM snapshot (bigfile)')
ap.add_argument('dest', help='Gadget filename base; dir will be created on the fly. ')
ap.add_argument('--time-ic', type=float, default=None, help='Time of IC of this simulation, defualt is the time of snapshot')
ap.add_argument('--unit-system', choices=['Mpc', 'Kpc'], default='Mpc')
ap.add_argument('--subsample', type=int, help='keep every n particles')

def main(ns):
    cat = Gadget1Catalog(ns.source, ptype=1)

    attrs = cat.attrs.copy()
    
    new_cat = cat.attrs.copy()
    cat.attrs.clear()
    
    cat.attrs['a.x'] = new_cat['Time']
    cat.attrs['a.v'] = new_cat['Time']
    cat.attrs['M0'] = new_cat['Massarr'][1]
    cat.save(ns.dest, columns=[], dataset='1', header='Header')
    odir = ns.dest+'/1'
    if not os.path.exists(odir):
        os.makedirs(odir)
    shutil.move(ns.dest+'/Header/attr-v2', ns.dest+'/1/attr-v2')
    
    cat.attrs.clear()

    cat.attrs['MassTable'] = attrs['Massarr']
    cat.attrs['TotNumPart'] = numpy.int64(attrs['Nall']) + (numpy.int64(attrs['NallHW']) << 32)
    cat.attrs['TotNumPartInit'] = numpy.int64(attrs['Nall']) + (numpy.int64(attrs['NallHW']) << 32)
    cat.attrs['BoxSize'] = attrs['BoxSize']
    cat.attrs['Time'] = attrs['Time']

    if ns.time_ic is None:
        ns.time_ic = attrs['Time']
    cat.attrs['TimeIC'] = ns.time_ic

    cat.attrs['UnitVelocity_in_cm_per_s'] = 1e5
    if ns.unit_system == 'Mpc':
        cat.attrs['UnitLength_in_cm'] = 3.085678e24
    if ns.unit_system == 'Kpc':
        cat.attrs['UnitLength_in_cm'] = 3.085678e21

    cat.attrs['UnitMass_in_g'] = 1.989e43

    # The velocity convention is weird without this
    cat.attrs['UsePeculiarVelocity'] = True

    a = attrs['Time']
    cat['Velocity'] = cat['GadgetVelocity'] * a ** 0.5

    if ns.subsample is not None:
        cat = cat[::ns.subsample]

    cat.save(ns.dest, columns=['Position', 'Velocity', 'ID'], dataset='1', header='Header')
    shutil.copy(ns.dest+'/Header/header', ns.dest+'/1/header')

if __name__ == '__main__':
    ns = ap.parse_args()

    main(ns)
