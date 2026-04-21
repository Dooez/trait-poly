add_executable(trp test_trp.cpp)
add_executable(trp_small test_trp_small.cpp)
add_executable(trp_cv_supertraits cv_supertraits.cpp)
add_executable(trp_default_trait_impl_examples default_trait_impl_examples.cpp)

target_link_libraries(trp				 PUBLIC "-static" trp::trp)
target_link_libraries(trp_small			 PUBLIC "-static" trp::trp)
target_link_libraries(trp_cv_supertraits PUBLIC "-static" trp::trp)
target_link_libraries(trp_default_trait_impl_examples PUBLIC "-static" trp::trp)
