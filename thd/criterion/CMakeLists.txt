include(FindPkgConfig)

pkg_search_module(criterion REQUIRED IMPORTED_TARGET GLOBAL criterion)

add_library(ntt::thd::criterion ALIAS PkgConfig::criterion)
