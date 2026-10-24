# hgis/CMakeLists.txt 의 qss 내장 방식을 그대로 따른다: ka-hgis.qss 를 8 KiB 이하 raw 문자열 조각으로
# 쪼개 ka-hgis.qss.inc 로 만든다(MSVC 문자열 길이 제한 때문). 원본은 hgis 의 data/theme/ka-hgis.qss.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${HGIS_ROOT}/data/theme/ka-hgis.qss")
file(READ "${HGIS_ROOT}/data/theme/ka-hgis.qss" KA_QSS_CONTENT)
set(_ka_qss_remaining "${KA_QSS_CONTENT}")
set(KA_QSS_LITERALS "")
while(TRUE)
  string(LENGTH "${_ka_qss_remaining}" _ka_qss_length)
  if(_ka_qss_length GREATER 8192)
    string(SUBSTRING "${_ka_qss_remaining}" 0 8192 _ka_qss_prefix)
    string(FIND "${_ka_qss_prefix}" "\n" _ka_qss_split REVERSE)
    if(_ka_qss_split LESS 0)
      message(FATAL_ERROR "ka-hgis.qss has a line longer than 8192 UTF-8 bytes.")
    endif()
    math(EXPR _ka_qss_chunk_length "${_ka_qss_split} + 1")
  else()
    set(_ka_qss_chunk_length ${_ka_qss_length})
  endif()
  string(SUBSTRING "${_ka_qss_remaining}" 0 ${_ka_qss_chunk_length} _ka_qss_chunk)
  string(FIND "${_ka_qss_chunk}" ")KAQSS\"" _ka_qss_delimiter)
  if(NOT _ka_qss_delimiter LESS 0)
    message(FATAL_ERROR "ka-hgis.qss contains the reserved raw-string delimiter.")
  endif()
  string(APPEND KA_QSS_LITERALS "R\"KAQSS(${_ka_qss_chunk})KAQSS\"\n")
  if(_ka_qss_length LESS_EQUAL 8192)
    break()
  endif()
  string(SUBSTRING "${_ka_qss_remaining}" ${_ka_qss_chunk_length} -1 _ka_qss_remaining)
endwhile()
configure_file(
  "${HGIS_ROOT}/src/app/ka-hgis.qss.inc.in"
  "${CMAKE_BINARY_DIR}/generated/ka-hgis.qss.inc"
  @ONLY
  NEWLINE_STYLE UNIX)
