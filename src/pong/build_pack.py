"""Use internal linkage for C++ inline functions to keep Blackfin relocation sections standard."""
from pathlib import Path
import sys
upstream = Path(sys.argv.pop(1))
sys.path[:0] = [str(upstream/'tools'), str(upstream/'open-xy')]
import pack_build, module_build
module_build.CXX_FLAGS += ['-fno-weak','-O3']
raise SystemExit(pack_build.main(sys.argv[1:]))
