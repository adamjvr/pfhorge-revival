#include "../../../Pfhorge Source/Format/Core/PfhorgeCanonicalLevel.hpp"
#include <cassert>
using namespace pfhorge::format;
int main(){ CanonicalLevel level; level.name="FORMAT-1C"; Point p; level.points.push_back(p); assert(level.points.size()==1); Media m; m.appearance.mode=MediaAppearanceMode::TypeDefault; level.media.push_back(m); return 0; }
