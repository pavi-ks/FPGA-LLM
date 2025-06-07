( return 0 2>/dev/null ) || \
{
  echo "Error: to run this script, type: source $0"; exit 1
}
if ! diff ./build_version.txt $COREDLA_ROOT/build_version.txt > /dev/null; then
  echo "Error: Environment is setup for an incompatible version of the AI Suite"
  echo "       Re-source the AI Suite init_env.sh from a compatible AI Suite installation."
  return 1
fi
echo export COREDLA_WORK='/root/DE10-Agilex/coredla_work'
export COREDLA_WORK='/root/DE10-Agilex/coredla_work'
