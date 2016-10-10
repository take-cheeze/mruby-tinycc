assert 'TCCState' do
  TCCState.new
end

assert 'TCCState#output_type=' do
  t = TCCState.new
  t.output_type = :memory
end

assert 'TCCState#run' do
  t = TCCState.new
  t.output_type = :memory
  t.compile_string <<EOS
int main() {
  return 314;
}
EOS
  assert_equal 314, t.run
end
