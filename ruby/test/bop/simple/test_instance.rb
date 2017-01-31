class Main
  @a="init a"
  @b="init b"
  @c="init c"
  @d="init d"

  def run()
    PPR{
      sleep(1)
      @a="a"
    }
    PPR{
      sleep(1)
      @b="b"
    }
    PPR{
      sleep(1)
      @c="c"
    }
    PPR{
      sleep(1)
      @d="d"
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
