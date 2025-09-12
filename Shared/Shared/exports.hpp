#pragma once

#include <unordered_map>
#include <string_view>
#include <print>

namespace NSA::Shared::Exports {
	constexpr std::string GetExportName(const std::string_view& signature) noexcept {
		assert(signature.size() > 5 && "Invalid signature size");
		assert(signature.starts_with('?') && "Signature must start with '?'");
		assert(signature.ends_with("Z") && "Signature must end with 'Z'");

		auto nameIndex = signature.find_first_not_of("?");
		assert(nameIndex != signature.npos && "Invalid signature");

		auto namespaceIndex = signature.find("@");
		assert(namespaceIndex != signature.npos && "Invalid signature");

		return std::string(signature.substr(nameIndex + 1, namespaceIndex - 1));
	}

	constexpr std::string GetNamespaceName(const std::string_view& signature) noexcept {
		assert(signature.size() > 5 && "Invalid signature size");
		assert(signature.starts_with('?') && "Signature must start with '?'");
		assert(signature.ends_with("Z") && "Signature must end with 'Z'");

		auto nameIndex = signature.find("@");
		assert(nameIndex != signature.npos && "Invalid signature");

		auto namespaceIndex = signature.find("@@");
		assert(namespaceIndex != signature.npos && "Invalid signature");

		if (namespaceIndex == nameIndex)
			return "";

		auto namespaceView = signature.substr(nameIndex + 1, namespaceIndex - nameIndex);
		std::string outNamespace;
		std::size_t prevPos = 0;

		for (std::size_t pos = 0; pos < namespaceView.size(); ++pos) {
			if (namespaceView[pos] == '@') {
				outNamespace += "::" + std::string(namespaceView.substr(prevPos, pos - prevPos));
				prevPos = pos + 1;
			}
		}

		return outNamespace.substr(2);
	}

	constexpr std::string GetCallingConvention(const std::string_view& signature) noexcept {
		assert(signature.size() > 5 && "Invalid signature size");
		assert(signature.starts_with('?') && "Signature must start with '?'");
		assert(signature.ends_with("Z") && "Signature must end with 'Z'");

		auto namespaceIndex = signature.find("@@");
		assert(namespaceIndex != signature.npos && "Invalid signature");

		switch (signature[namespaceIndex + 3]) {
			case 'A': return "__cdecl";
			case 'X': return "__stdcall";
			case 'F': return "__fastcall";
			case 'D': return "__vectorcall";
			default: return "__unknown";
		}
	}

	constexpr std::string GetPrimitiveType(
		const std::string_view& view,
		std::size_t* position = nullptr
	) noexcept {
		assert(!view.empty() && "Invalid signature");

		const auto SetPosition = [&position](std::size_t pos) {
			if (position) *position = pos;
		};

		switch (view[0]) {
			case 'D': SetPosition(1); return "char";
			case 'F': SetPosition(1); return "short";
			case 'H': SetPosition(1); return "int";
			case 'J': SetPosition(1); return "long";
			case 'M': SetPosition(1); return "float";
			case 'N': SetPosition(1); return "double";
			case 'O': SetPosition(1); return "long double";
			case '_':
				switch (view[1]) {
					case 'N': SetPosition(2); return "bool";
					case 'J': SetPosition(2); return "long long";
				}
				break;
			case '?': {
				std::string modifier;
				switch (view[1]) {
					case 'A': modifier = ""; break;
					case 'B': modifier = "const "; break;
					case 'C': modifier = "volatile "; break;
					case 'D': modifier = "const volatile "; break;
				}
				std::size_t pos = 0;
				auto res = GetPrimitiveType(view.substr(2), &pos);
				SetPosition(pos + 2);
				return modifier + res;
			}
			case 'P':
			case 'Q':
			case 'R':
			case 'S': {
				assert(view.size() > 3 && view[1] == 'E' && "Invalid signature");

				std::string out;
				switch (view[2]) {
					case 'A': out = ""; break;
					case 'B': out = "const "; break;
					case 'C': out = "volatile "; break;
					case 'D': out = "const volatile "; break;
				}
				
				std::size_t pos = 0;
				out += GetPrimitiveType(view.substr(3), &pos);
				SetPosition(pos + 3);

				switch (view[0]) {
					case 'P': out += "*"; break;
					case 'Q': out += "* const"; break;
					case 'R': out += "* volatile"; break;
					case 'S': out += "* const volatile"; break;
				}
				return out;
			}
			case 'T':
			case 'U': {
				assert(view.size() > 3 && view[1] == 't' && "Invalid signature");

				auto index = view.find("@");
				
				std::string type;
				switch (view[0]) {
					case 'T': type = "union "; break;
					case 'U': type = "struct "; break;
				}
				auto index2 = view.find("@", index + 1);
				SetPosition(index2 + 1);
				return type + std::string(view.substr(1, index - 1));
			}
			case 'X': {
				SetPosition(1);
				return "void";
			}
			case 'Z': {
				SetPosition(1);
				return "...";
			}
		}
		SetPosition(1);
		return "unknown";
	}

	constexpr std::string GetReturnType(const std::string_view& signature) noexcept {
		assert(signature.size() > 5 && "Invalid signature size");
		assert(signature.starts_with('?') && "Signature must start with '?'");
		assert(signature.ends_with("Z") && "Signature must end with 'Z'");

		auto namespaceIndex = signature.find("@@");
		assert(namespaceIndex != signature.npos && "Invalid signature");

		return GetPrimitiveType(signature.substr(namespaceIndex + 4));
	}

	constexpr std::string GetParamsType(const std::string_view& signature) noexcept {
		assert(signature.size() > 5 && "Invalid signature size");
		assert(signature.starts_with('?') && "Signature must start with '?'");
		assert(signature.ends_with("Z") && "Signature must end with 'Z'");

		auto namespaceIndex = signature.find("@@");
		assert(namespaceIndex != signature.npos && "Invalid signature");

		std::size_t pos = 0;
		GetPrimitiveType(signature.substr(namespaceIndex + 4), &pos);
		pos += namespaceIndex + 4;
		std::string out;

		std::vector<std::string> params;
		std::string_view view = signature.substr(pos);
		pos = 0;

		while (!view.empty()) {
			if (view[0] == 'Z') {
				if (view.size() == 1)
					break;
				params.push_back("...");
				break;
			}
			auto res = GetPrimitiveType(view.substr(pos), &pos);
			params.push_back(res);
			view = view.substr(pos);
			pos = 0;
		}
		for (auto i = 0; i < params.size(); i++) {
			if (!out.empty())
				out += ", ";
			out += params[i];
		}

		return out;
	}

	constexpr std::string GetDemangledName(const std::string_view& signature) noexcept {
		auto returnType = GetReturnType(signature);
		auto callingConvention = GetCallingConvention(signature);
		auto namespaceName = GetNamespaceName(signature);
		auto exportName = GetExportName(signature);
		auto paramsType = GetParamsType(signature);

		return returnType + " " + callingConvention + " " +
			namespaceName + "::" + exportName + "(" + paramsType + ")";
	}

	constexpr std::string DemangleClassFunction(const std::string_view& signature) noexcept {

		return "";
	}
}

/*
??4t_struct@@QEAAAEAU0@$$QEAU0@@Z
??4t_struct@@QEAAAEAU0@AEBU0@@Z
 
// --------------------- \\

?export_t_bool@Namespace@Test@@YA_NPEBDZZ     -> _NPEBDZ
?export_t_char@Namespace@Test@@YADPEBDZZ      -> DPEBDZ
?export_t_short@Namespace@Test@@YAFPEBDZZ     -> FPEBDZ
?export_t_int@Namespace@Test@@YAHPEBDZZ       -> HPEBDZ
?export_t_long@Namespace@Test@@YAJPEBDZZ      -> JPEBDZ
?export_t_longlong@Namespace@Test@@YA_JPEBDZZ -> _JPEBDZ
?export_t_void@Namespace@Test@@YAXPEBDZZ      -> XPEBDZ

?export_t_struct@Namespace@Test@@YA?AUt_struct@12@PEBDZZ -> ?A Ut_struct@12@PEBDZ
?export_t_union@Namespace@Test@@YA?ATt_union@12@PEBDZZ   -> ?A Tt_union@12@PEBDZ

// --------------------- \\

?export_t_constbool@Namespace@Test@@YA?B_NPEBDZZ     -> ?B_NPEBDZ
?export_t_constchar@Namespace@Test@@YA?BDPEBDZZ      -> ?BDPEBDZ
?export_t_constshort@Namespace@Test@@YA?BFPEBDZZ     -> ?BFPEBDZ
?export_t_constint@Namespace@Test@@YA?BHPEBDZZ       -> ?BHPEBDZ
?export_t_constlong@Namespace@Test@@YA?BJPEBDZZ      -> ?BJPEBDZ
?export_t_constlonglong@Namespace@Test@@YA?B_JPEBDZZ -> ?B_JPEBDZ
?export_t_constvoid@Namespace@Test@@YAXPEBDZZ        -> XPEBDZ

?export_t_conststruct@Namespace@Test@@YA?BUt_struct@12@PEBDZZ -> ?BU t_struct@12@PEBDZ
?export_t_constunion@Namespace@Test@@YA?BTt_union@12@PEBDZZ   -> ?BT t_union@12@PEBDZ

// --------------------- \\

?export_t_volatilebool@Namespace@Test@@YA?C_NPEBDZZ     -> ?C_NPEBDZ
?export_t_volatilechar@Namespace@Test@@YA?CDPEBDZZ      -> ?CDPEBDZ
?export_t_volatileshort@Namespace@Test@@YA?CFPEBDZZ     -> ?CFPEBDZ
?export_t_volatileint@Namespace@Test@@YA?CHPEBDZZ       -> ?CHPEBDZ
?export_t_volatilelong@Namespace@Test@@YA?CJPEBDZZ      -> ?CJPEBDZ
?export_t_volatilelonglong@Namespace@Test@@YA?C_JPEBDZZ -> ?C_JPEBDZ
?export_t_volatilevoid@Namespace@Test@@YAXPEBDZZ        -> XPEBDZ

?export_t_volatilestruct@Namespace@Test@@YA?CUt_struct@12@PEBDZZ -> ?C Ut_struct@12@PEBDZ
?export_t_volatileunion@Namespace@Test@@YA?CTt_union@12@PEBDZZ   -> ?C Tt_union@12@PEBDZ

// --------------------- \\

?export_t_constvolatilebool@Namespace@Test@@YA?D_NPEBDZZ     -> ?D_NPEBDZ
?export_t_constvolatilechar@Namespace@Test@@YA?DDPEBDZZ      -> ?DDPEBDZ
?export_t_constvolatileshort@Namespace@Test@@YA?DFPEBDZZ     -> ?DFPEBDZ
?export_t_constvolatileint@Namespace@Test@@YA?DHPEBDZZ       -> ?DHPEBDZ
?export_t_constvolatilelong@Namespace@Test@@YA?DJPEBDZZ      -> ?DJPEBDZ
?export_t_constvolatilelonglong@Namespace@Test@@YA?D_JPEBDZZ -> ?D_JPEBDZ
?export_t_constvolatilevoid@Namespace@Test@@YAXPEBDZZ        -> XPEBDZ

?export_t_constvolatilestruct@Namespace@Test@@YA?DUt_struct@12@PEBDZZ -> ?D Ut_struct@12@PEBDZ
?export_t_constvolatileunion@Namespace@Test@@YA?DTt_union@12@PEBDZZ   -> ?D Tt_union@12@PEBDZ

// --------------------- \\

?export_t_boolptr@Namespace@Test@@YAPEA_NPEBDZZ              -> PEA_NPEBDZ
?export_t_boolconstptr@Namespace@Test@@YAQEA_NPEBDZZ         -> QEA_NPEBDZ
?export_t_boolvolatileptr@Namespace@Test@@YAREA_NPEBDZZ      -> REA_NPEBDZ
?export_t_boolconstvolatileptr@Namespace@Test@@YASEA_NPEBDZZ -> SEA_NPEBDZ

?export_t_charptr@Namespace@Test@@YAPEADPEBDZZ              -> PEADPEBDZ
?export_t_charconstptr@Namespace@Test@@YAQEADPEBDZZ         -> QEADPEBDZ
?export_t_charvolatileptr@Namespace@Test@@YAREADPEBDZZ      -> READPEBDZ
?export_t_charconstvolatileptr@Namespace@Test@@YASEADPEBDZZ -> SEADPEBDZ

?export_t_shortptr@Namespace@Test@@YAPEAFPEBDZZ              -> PEAFPEBDZ
?export_t_shortconstptr@Namespace@Test@@YAQEAFPEBDZZ         -> QEAFPEBDZ
?export_t_shortvolatileptr@Namespace@Test@@YAREAFPEBDZZ      -> REAFPEBDZ
?export_t_shortconstvolatileptr@Namespace@Test@@YASEAFPEBDZZ -> SEAFPEBDZ

?export_t_intptr@Namespace@Test@@YAPEAHPEBDZZ              -> PEAHPEBDZ
?export_t_intconstptr@Namespace@Test@@YAQEAHPEBDZZ         -> QEAHPEBDZ
?export_t_intvolatileptr@Namespace@Test@@YAREAHPEBDZZ      -> REAHPEBDZ
?export_t_intconstvolatileptr@Namespace@Test@@YASEAHPEBDZZ -> SEAHPEBDZ

?export_t_longptr@Namespace@Test@@YAPEAJPEBDZZ              -> PEAJPEBDZ
?export_t_longconstptr@Namespace@Test@@YAQEAJPEBDZZ         -> QEAJPEBDZ
?export_t_longvolatileptr@Namespace@Test@@YAREAJPEBDZZ      -> REAJPEBDZ
?export_t_longconstvolatileptr@Namespace@Test@@YASEAJPEBDZZ -> SEAJPEBDZ

?export_t_longlongptr@Namespace@Test@@YAPEA_JPEBDZZ              -> PEA_JPEBDZ
?export_t_longlongconstptr@Namespace@Test@@YAQEA_JPEBDZZ         -> QEA_JPEBDZ
?export_t_longlongvolatileptr@Namespace@Test@@YAREA_JPEBDZZ      -> REA_JPEBDZ
?export_t_longlongconstvolatileptr@Namespace@Test@@YASEA_JPEBDZZ -> SEA_JPEBDZ

?export_t_voidptr@Namespace@Test@@YAPEAXPEBDZZ              -> PEAXPEBDZ
?export_t_voidconstptr@Namespace@Test@@YAQEAXPEBDZZ         -> QEAXPEBDZ
?export_t_voidvolatileptr@Namespace@Test@@YAREAXPEBDZZ      -> REAXPEBDZ
?export_t_voidconstvolatileptr@Namespace@Test@@YASEAXPEBDZZ -> SEAXPEBDZ

?export_t_structptr@Namespace@Test@@YAPEAUt_struct@12@PEBDZZ              -> PEA Ut_struct@12@PEBDZ
?export_t_structconstptr@Namespace@Test@@YAQEAUt_struct@12@PEBDZZ         -> QEA Ut_struct@12@PEBDZ
?export_t_structvolatileptr@Namespace@Test@@YAREAUt_struct@12@PEBDZZ      -> REA Ut_struct@12@PEBDZ
?export_t_structconstvolatileptr@Namespace@Test@@YASEAUt_struct@12@PEBDZZ -> SEA Ut_struct@12@PEBDZ

?export_t_unionptr@Namespace@Test@@YAPEATt_union@12@PEBDZZ              -> PEA Tt_union@12@PEBDZ
?export_t_unionconstptr@Namespace@Test@@YAQEATt_union@12@PEBDZZ         -> QEA Tt_union@12@PEBDZ
?export_t_unionvolatileptr@Namespace@Test@@YAREATt_union@12@PEBDZZ      -> REA Tt_union@12@PEBDZ
?export_t_unionconstvolatileptr@Namespace@Test@@YASEATt_union@12@PEBDZZ -> SEA Tt_union@12@PEBDZ

// --------------------- \\

?export_t_constboolptr@Namespace@Test@@YAPEB_NPEBDZZ              -> PEB_NPEBDZ
?export_t_constboolconstptr@Namespace@Test@@YAQEB_NPEBDZZ         -> QEB_NPEBDZ
?export_t_constboolvolatileptr@Namespace@Test@@YAREB_NPEBDZZ      -> REB_NPEBDZ
?export_t_constboolconstvolatileptr@Namespace@Test@@YASEB_NPEBDZZ -> SEB_NPEBDZ

?export_t_constcharptr@Namespace@Test@@YAPEBDPEBDZZ              -> PEBDPEBDZ
?export_t_constcharconstptr@Namespace@Test@@YAQEBDPEBDZZ         -> QEBDPEBDZ
?export_t_constcharvolatileptr@Namespace@Test@@YAREBDPEBDZZ      -> REBDPEBDZ
?export_t_constcharconstvolatileptr@Namespace@Test@@YASEBDPEBDZZ -> SEBDPEBDZ

?export_t_constshortptr@Namespace@Test@@YAPEBFPEBDZZ              -> PEBFPEBDZ
?export_t_constshortconstptr@Namespace@Test@@YAQEBFPEBDZZ         -> QEBFPEBDZ
?export_t_constshortvolatileptr@Namespace@Test@@YAREBFPEBDZZ      -> REBFPEBDZ
?export_t_constshortconstvolatileptr@Namespace@Test@@YASEBFPEBDZZ -> SEBFPEBDZ

?export_t_constintptr@Namespace@Test@@YAPEBHPEBDZZ              -> PEBHPEBDZ
?export_t_constintconstptr@Namespace@Test@@YAQEBHPEBDZZ         -> QEBHPEBDZ
?export_t_constintvolatileptr@Namespace@Test@@YAREBHPEBDZZ      -> REBHPEBDZ
?export_t_constintconstvolatileptr@Namespace@Test@@YASEBHPEBDZZ -> SEBHPEBDZ

?export_t_constlongptr@Namespace@Test@@YAPEBJPEBDZZ              -> PEBJPEBDZ
?export_t_constlongconstptr@Namespace@Test@@YAQEBJPEBDZZ         -> QEBJPEBDZ
?export_t_constlongvolatileptr@Namespace@Test@@YAREBJPEBDZZ      -> REBJPEBDZ
?export_t_constlongconstvolatileptr@Namespace@Test@@YASEBJPEBDZZ -> SEBJPEBDZ

?export_t_constlonglongptr@Namespace@Test@@YAPEB_JPEBDZZ              -> PEB_JPEBDZ
?export_t_constlonglongconstptr@Namespace@Test@@YAQEB_JPEBDZZ         -> QEB_JPEBDZ
?export_t_constlonglongvolatileptr@Namespace@Test@@YAREB_JPEBDZZ      -> REB_JPEBDZ
?export_t_constlonglongconstvolatileptr@Namespace@Test@@YASEB_JPEBDZZ -> SEB_JPEBDZ

?export_t_constvoidptr@Namespace@Test@@YAPEBXPEBDZZ              -> PEBXPEBDZ
?export_t_constvoidconstptr@Namespace@Test@@YAQEBXPEBDZZ         -> QEBXPEBDZ
?export_t_constvoidvolatileptr@Namespace@Test@@YAREBXPEBDZZ      -> REBXPEBDZ
?export_t_constvoidconstvolatileptr@Namespace@Test@@YASEBXPEBDZZ -> SEBXPEBDZ

?export_t_conststructptr@Namespace@Test@@YAPEBUt_struct@12@PEBDZZ              -> PEB Ut_struct@12@PEBDZ
?export_t_conststructconstptr@Namespace@Test@@YAQEBUt_struct@12@PEBDZZ         -> QEB Ut_struct@12@PEBDZ
?export_t_conststructvolatileptr@Namespace@Test@@YAREBUt_struct@12@PEBDZZ      -> REB Ut_struct@12@PEBDZ
?export_t_conststructconstvolatileptr@Namespace@Test@@YASEBUt_struct@12@PEBDZZ -> SEB Ut_struct@12@PEBDZ

?export_t_constunionptr@Namespace@Test@@YAPEBTt_union@12@PEBDZZ              -> PEB Tt_union@12@PEBDZ
?export_t_constunionconstptr@Namespace@Test@@YAQEBTt_union@12@PEBDZZ         -> QEB Tt_union@12@PEBDZ
?export_t_constunionvolatileptr@Namespace@Test@@YAREBTt_union@12@PEBDZZ      -> REB Tt_union@12@PEBDZ
?export_t_constunionconstvolatileptr@Namespace@Test@@YASEBTt_union@12@PEBDZZ -> SEB Tt_union@12@PEBDZ

// --------------------- \\

?export_t_volatileboolptr@Namespace@Test@@YAPEC_NPEBDZZ              -> PEC_NPEBDZ
?export_t_volatileboolconstptr@Namespace@Test@@YAQEC_NPEBDZZ         -> QEC_NPEBDZ
?export_t_volatileboolvolatileptr@Namespace@Test@@YAREC_NPEBDZZ      -> REC_NPEBDZ
?export_t_volatileboolconstvolatileptr@Namespace@Test@@YASEC_NPEBDZZ -> SEC_NPEBDZ

?export_t_volatilecharptr@Namespace@Test@@YAPECDPEBDZZ              -> PECDPEBDZ
?export_t_volatilecharconstptr@Namespace@Test@@YAQECDPEBDZZ         -> QECDPEBDZ
?export_t_volatilecharvolatileptr@Namespace@Test@@YARECDPEBDZZ      -> RECDPEBDZ
?export_t_volatilecharconstvolatileptr@Namespace@Test@@YASECDPEBDZZ -> SECDPEBDZ

?export_t_volatileshortptr@Namespace@Test@@YAPECFPEBDZZ              -> PECFPEBDZ
?export_t_volatileshortconstptr@Namespace@Test@@YAQECFPEBDZZ         -> QECFPEBDZ
?export_t_volatileshortvolatileptr@Namespace@Test@@YARECFPEBDZZ      -> RECFPEBDZ
?export_t_volatileshortconstvolatileptr@Namespace@Test@@YASECFPEBDZZ -> SECFPEBDZ

?export_t_volatileintptr@Namespace@Test@@YAPECHPEBDZZ              -> PECHPEBDZ
?export_t_volatileintconstptr@Namespace@Test@@YAQECHPEBDZZ         -> QECHPEBDZ
?export_t_volatileintvolatileptr@Namespace@Test@@YARECHPEBDZZ      -> RECHPEBDZ
?export_t_volatileintconstvolatileptr@Namespace@Test@@YASECHPEBDZZ -> SECHPEBDZ

?export_t_volatilelongptr@Namespace@Test@@YAPECJPEBDZ               -> PECJPEBD
?export_t_volatilelongconstptr@Namespace@Test@@YAQECJPEBDZZ         -> QECJPEBDZ
?export_t_volatilelongvolatileptr@Namespace@Test@@YARECJPEBDZZ      -> RECJPEBDZ
?export_t_volatilelongconstvolatileptr@Namespace@Test@@YASECJPEBDZZ -> SECJPEBDZ

?export_t_volatilelonglongptr@Namespace@Test@@YAPEC_JPEBDZZ              -> PEC_JPEBDZ
?export_t_volatilelonglongconstptr@Namespace@Test@@YAQEC_JPEBDZZ         -> QEC_JPEBDZ
?export_t_volatilelonglongvolatileptr@Namespace@Test@@YAREC_JPEBDZZZ     -> REC_JPEBDZZ
?export_t_volatilelonglongconstvolatileptr@Namespace@Test@@YASEC_JPEBDZZ -> SEC_JPEBDZ

?export_t_volatilevoidptr@Namespace@Test@@YAPECXPEBDZZ              -> PECXPEBDZ
?export_t_volatilevoidconstptr@Namespace@Test@@YAQECXPEBDZZ         -> QECXPEBDZ
?export_t_volatilevoidvolatileptr@Namespace@Test@@YARECXPEBDZZ      -> RECXPEBDZ
?export_t_volatilevoidconstvolatileptr@Namespace@Test@@YASECXPEBDZZ -> SECXPEBDZ

?export_t_volatilestructptr@Namespace@Test@@YAPECUt_struct@12@PEBDZZ              -> PEC Ut_struct@12@PEBDZ
?export_t_volatilestructconstptr@Namespace@Test@@YAQECUt_struct@12@PEBDZZ         -> QEC Ut_struct@12@PEBDZ
?export_t_volatilestructvolatileptr@Namespace@Test@@YARECUt_struct@12@PEBDZZ      -> REC Ut_struct@12@PEBDZ
?export_t_volatilestructconstvolatileptr@Namespace@Test@@YASECUt_struct@12@PEBDZZ -> SEC Ut_struct@12@PEBDZ

?export_t_volatileunionptr@Namespace@Test@@YAPECTt_union@12@PEBDZZ              -> PEC Tt_union@12@PEBDZ
?export_t_volatileunionconstptr@Namespace@Test@@YAQECTt_union@12@PEBDZZ         -> QEC Tt_union@12@PEBDZ
?export_t_volatileunionvolatileptr@Namespace@Test@@YARECTt_union@12@PEBDZZ      -> REC Tt_union@12@PEBDZ
?export_t_volatileunionconstvolatileptr@Namespace@Test@@YASECTt_union@12@PEBDZZ -> SEC Tt_union@12@PEBDZ

// --------------------- \\

?export_t_constvolatileboolptr@Namespace@Test@@YAPED_NPEBDZZ              -> PED_NPEBDZ
?export_t_constvolatileboolconstptr@Namespace@Test@@YAQED_NPEBDZZ         -> QED_NPEBDZ
?export_t_constvolatileboolvolatileptr@Namespace@Test@@YARED_NPEBDZZ      -> RED_NPEBDZ
?export_t_constvolatileboolconstvolatileptr@Namespace@Test@@YASED_NPEBDZZ -> SED_NPEBDZ

?export_t_constvolatilecharptr@Namespace@Test@@YAPEDDPEBDZZ              -> PEDDPEBDZ
?export_t_constvolatilecharconstptr@Namespace@Test@@YAQEDDPEBDZZ         -> QEDDPEBDZ
?export_t_constvolatilecharvolatileptr@Namespace@Test@@YAREDDPEBDZZ      -> REDDPEBDZ
?export_t_constvolatilecharconstvolatileptr@Namespace@Test@@YASEDDPEBDZZ -> SEDDPEBDZ

?export_t_constvolatileshortptr@Namespace@Test@@YAPEDFPEBDZZ              -> PEDFPEBDZ
?export_t_constvolatileshortconstptr@Namespace@Test@@YAQEDFPEBDZZ         -> QEDFPEBDZ
?export_t_constvolatileshortvolatileptr@Namespace@Test@@YAREDFPEBDZZ      -> REDFPEBDZ
?export_t_constvolatileshortconstvolatileptr@Namespace@Test@@YASEDFPEBDZZ -> SEDFPEBDZ

?export_t_constvolatileintptr@Namespace@Test@@YAPEDHPEBDZZ              -> PEDHPEBDZ
?export_t_constvolatileintconstptr@Namespace@Test@@YAQEDHPEBDZZ         -> QEDHPEBDZ
?export_t_constvolatileintvolatileptr@Namespace@Test@@YAREDHPEBDZZ      -> REDHPEBDZ
?export_t_constvolatileintconstvolatileptr@Namespace@Test@@YASEDHPEBDZZ -> SEDHPEBDZ

?export_t_constvolatilelongptr@Namespace@Test@@YAPEDJPEBDZZ              -> PEDJPEBDZ
?export_t_constvolatilelongconstptr@Namespace@Test@@YAQEDJPEBDZZ         -> QEDJPEBDZ
?export_t_constvolatilelongvolatileptr@Namespace@Test@@YAREDJPEBDZZ      -> REDJPEBDZ
?export_t_constvolatilelongconstvolatileptr@Namespace@Test@@YASEDJPEBDZZ -> SEDJPEBDZ

?export_t_constvolatilelonglongptr@Namespace@Test@@YAPED_JPEBDZZ              -> PED_JPEBDZ
?export_t_constvolatilelonglongconstptr@Namespace@Test@@YAQED_JPEBDZZ         -> QED_JPEBDZ
?export_t_constvolatilelonglongvolatileptr@Namespace@Test@@YARED_JPEBDZZ      -> RED_JPEBDZ
?export_t_constvolatilelonglongconstvolatileptr@Namespace@Test@@YASED_JPEBDZZ -> SED_JPEBDZ

?export_t_constvolatilevoidptr@Namespace@Test@@YAPEDXPEBDZZ              -> PEDXPEBDZ
?export_t_constvolatilevoidconstptr@Namespace@Test@@YAQEDXPEBDZZ         -> QEDXPEBDZ
?export_t_constvolatilevoidvolatileptr@Namespace@Test@@YAREDXPEBDZZ      -> REDXPEBDZ
?export_t_constvolatilevoidconstvolatileptr@Namespace@Test@@YASEDXPEBDZZ -> SEDXPEBDZ

?export_t_constvolatilestructptr@Namespace@Test@@YAPEDUt_struct@12@PEBDZZ              -> PED Ut_struct@12@PEBDZ
?export_t_constvolatilestructconstptr@Namespace@Test@@YAQEDUt_struct@12@PEBDZZ         -> QED Ut_struct@12@PEBDZ
?export_t_constvolatilestructvolatileptr@Namespace@Test@@YAREDUt_struct@12@PEBDZZ      -> RED Ut_struct@12@PEBDZ
?export_t_constvolatilestructconstvolatileptr@Namespace@Test@@YASEDUt_struct@12@PEBDZZ -> SED Ut_struct@12@PEBDZ

?export_t_constvolatileunionptr@Namespace@Test@@YAPEDTt_union@12@PEBDZZ              -> PED Tt_union@12@PEBDZ
?export_t_constvolatileunionconstptr@Namespace@Test@@YAQEDTt_union@12@PEBDZZ         -> QED Tt_union@12@PEBDZ
?export_t_constvolatileunionvolatileptr@Namespace@Test@@YAREDTt_union@12@PEBDZZ      -> RED Tt_union@12@PEBDZ
?export_t_constvolatileunionconstvolatileptr@Namespace@Test@@YASEDTt_union@12@PEBDZZ -> SED Tt_union@12@PEBDZ

// --------------------- \\

*/