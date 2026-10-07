#include "TabTechniqueCompiler.h"

namespace tabplayer
{

bool TabTechniqueCompiler::compile (const TabToken& token, ScoreTechnique& out) noexcept
{
    if (token.kind != TabTokenKind::technique)
        return false;

    using Type = ScoreTechnique::Type;
    const auto meaning = token.canonicalMeaning.trim();

    out = {};

    if      (meaning == "bend")               out.type = Type::bend;
    else if (meaning == "bendRelease")        out.type = Type::bendRelease;
    else if (meaning == "preBend")            out.type = Type::preBend;
    else if (meaning == "slideUp")            out.type = Type::slideUp;
    else if (meaning == "slideDown")          out.type = Type::slideDown;
    else if (meaning == "slideLegato")        out.type = Type::slideLegato;
    else if (meaning == "slideShift")         out.type = Type::slideShift;
    else if (meaning == "slideIn")            out.type = Type::slideIn;
    else if (meaning == "slideOut")           out.type = Type::slideOut;
    else if (meaning == "hammerOn")           out.type = Type::hammerOn;
    else if (meaning == "pullOff")            out.type = Type::pullOff;
    else if (meaning == "palmMute")           out.type = Type::palmMute;
    else if (meaning == "deadNote")           out.type = Type::deadNote;
    else if (meaning == "naturalHarmonic")    out.type = Type::naturalHarmonic;
    else if (meaning == "pinchHarmonic")      out.type = Type::pinchHarmonic;
    else if (meaning == "artificialHarmonic") out.type = Type::artificialHarmonic;
    else if (meaning == "tapHarmonic")        out.type = Type::tapHarmonic;
    else if (meaning == "tap")                out.type = Type::tap;
    else if (meaning == "vibrato")            out.type = Type::vibrato;
    else if (meaning == "trill")              out.type = Type::trill;
    else if (meaning == "whammy")             out.type = Type::whammy;
    else if (meaning == "ghostNote")          out.type = Type::ghostNote;
    else if (meaning == "accent")             out.type = Type::accent;
    else if (meaning == "staccato")           out.type = Type::staccato;
    else if (meaning == "letRing")            out.type = Type::letRing;
    else if (meaning == "pickStrokeUp")       out.type = Type::pickStrokeUp;
    else if (meaning == "pickStrokeDown")     out.type = Type::pickStrokeDown;
    else if (meaning == "slap")               out.type = Type::slap;
    else if (meaning == "pop")                out.type = Type::pop;
    else                                          return false;

    out.value = token.numericValue;
    return true;
}

} // namespace tabplayer
