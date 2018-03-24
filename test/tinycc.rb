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
  t.nostdinc = true
  t.nostdlib = true
  t.verbose = true
  # t.error_callback{ |str| __t_printstr__ "#{str}\n" }
  t.error_callback{ |str| raise str }
  t.compile_string <<EOS
int main() {
  return 127;
}
EOS
  assert_equal 127, t.run
end
