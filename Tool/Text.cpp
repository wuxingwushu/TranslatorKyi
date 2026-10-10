#include "Text.h"

namespace TOOL {

	std::string ReplaceTokens(const std::string& Tpl, const std::vector<std::string>& Values) {
		std::string Result;
		Result.reserve(Tpl.size() + 32);
		size_t ValueIndex = 0;
		for (size_t i = 0; i < Tpl.size(); i++) {
			const bool IsToken =
				(Tpl[i] == '%') && ((i + 1) < Tpl.size()) &&
				((Tpl[i + 1] == 's') || (Tpl[i + 1] == 'd')) &&
				(ValueIndex < Values.size());
			if (IsToken) {
				Result += Values[ValueIndex];
				ValueIndex++;
				i++;
			}
			else {
				Result += Tpl[i];
			}
		}
		return Result;
	}

	std::string ReplaceToken(const std::string& Tpl, const std::string& Value) {
		return ReplaceTokens(Tpl, { Value });
	}

}
