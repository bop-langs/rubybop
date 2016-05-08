class Main
  @a="init a"
  @b="init b"
  @c="init c"
  @d="init d"

  def run()
    PPR{
      sleep(2)
      @a=1
    }
    PPR{
      sleep(2)
      @b=2
    }
    PPR{
      sleep(2)
      @a=3
    }
    PPR{
      sleep(2)
      @d=4
    }
    PPR.over
    return self
  end

  def pl()
    puts @a
    puts @b
    puts @c
    puts @d

  end
end
Main.new().run().pl()
