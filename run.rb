MODS = {
  8 => [
    251, 241, 239, 233, 229, 227, 223, 211, 199, 197,
    193, 191, 181, 179, 173, 167, 163, 157, 151, 149,
    139, 137, 131, 127, 113, 109, 107, 103, 101, 97,
    89, 83, 79, 73, 71, 67, 61, 59, 53, 47,
    43, 41, 37, 31, 29, 23, 19, 17, 13, 11,
    7, 5, 3, 2
  ],
  16 => [
    65521, 65519, 65497, 65479, 65449, 65447, 65437, 65423, 65419, 65413,
    65407, 65393, 65381, 65371, 65357, 65353, 65327, 65323, 65309, 65293,
    65287, 65269, 65267, 65257, 65239, 65213, 65203, 65183, 65179, 65173,
    65171, 65167, 65147, 65141, 65129, 65123, 65119, 65111, 65101, 65099,
    65089, 65071, 65063, 65053, 65033, 65029, 65027, 65011, 65003, 64997
  ],
  32 => [
    4294966661, 4294966657, 4294966651, 4294966639, 4294966619,
    4294966591, 4294966583, 4294966553, 4294966477, 4294966447,
    4294966441, 4294966427, 4294966373, 4294966367, 4294966337,
    4294966297, 4294966243, 4294966237, 4294966231, 4294966217,
    4294966187, 4294966177, 4294966163, 4294966153, 4294966129,
    4294966121, 4294966099, 4294966087, 4294966073, 4294966043
  ],
  64 => [
    9223372036854775783, 9223372036854775643, 9223372036854775549,
    9223372036854775507, 9223372036854775433, 9223372036854775421,
    9223372036854775417, 9223372036854775399, 9223372036854775351,
    9223372036854775337, 9223372036854775291, 9223372036854775279,
    9223372036854775259, 9223372036854775181, 9223372036854775159,
    9223372036854775139, 9223372036854775097, 9223372036854775073
  ]
}

COMMAND = 'path-counter'
if ARGV.length < 3
  puts "usage: n bits threads [cycles | hamiltonian]"
  return
end

n = ARGV[0].to_i
bits = ARGV[1].to_i
threads = ARGV[2].to_i
hamiltonian = ARGV[3] == 'hamiltonian'
cycles = hamiltonian || ARGV[3] == 'cycles'

if n < 4 || n > 30
  puts 'n is out of range [4, 30]'
  return
end

# compile
`make N=#{n} BITS=#{bits} CYCLES=#{cycles ? 1 : 0} HAMILTONIAN=#{hamiltonian ? 1 : 0} N_THREADS=#{threads}`
raise 'compile error' unless $?.success?

##########################################

def invmod(a, b)
  old_r, r = a, b
  old_s, s = 1, 0

  while r != 0 do
    q = old_r / r
    old_r, r = r, old_r - q * r
    old_s, s = s, old_s - q * s
  end
  old_s
end

def crt(remainders, primes)
  return remainders.first if remainders.length == 1

  s = 0
  primes.each_with_index do |p, i|
    pp = (primes - [p]).inject(:*)
    s += remainders[i] * pp * invmod(pp, p)
  end
  s % primes.inject(:*)
end

##########################################

results = []
used_mods = []
current_result = 0
final_result = 0

MODS[bits].each_with_index do |mod, i|
  last_line = ""
  buffer = ""
  IO.popen("./#{COMMAND} #{mod}", "r") do |io|
    io.each_char do |char|
      print char
      $stdout.flush
      if char == "\n"
        last_line = buffer unless buffer.empty?
        buffer = ""
      else
        buffer << char
      end
    end
  end
  raise 'execution error' if last_line.empty?

  parts = last_line.split
  result = parts[2].to_i
  used_mod = parts[4].to_i

  raise "mod mismatch #{used_mod} != #{mod}" if used_mod != mod

  results << result
  used_mods << used_mod
  new_result = crt(results, used_mods)

  puts "results: #{results}"
  puts "mods: #{used_mods}"

  puts "step: #{i + 1}, mod: #{mod}, result: #{new_result}"
  puts '-'  * 40

  if current_result == new_result
    final_result = current_result
    break
  end
  current_result = new_result
end

if final_result != 0
  puts "final result: #{final_result}"
else
  puts "failed to find a solution"
end
